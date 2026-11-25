#include "envelope.hpp"
#include "openember/logging/logger.hpp"
#include "openember/hardware/timestamp.hpp"
#include "openember/sensor/imu/calibration.hpp"
#include "openember/transport/service_server.hpp"
#include <deque>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>

namespace openember::sensor::link {
namespace hw = openember::hardware;
namespace t = openember::transport;
using Bytes = t::ByteBuffer;
static std::uint64_t Now() { return hw::SteadyTimeNs(); }
static void Require(bool ok,const std::string& why) { if(!ok)throw std::runtime_error(why); }
namespace {
struct Lease { std::string source,token; std::uint64_t until=0; };
struct Job { std::string source,encoded; wire::ImuRequest request; wire::ImuReply reply; std::uint64_t completed=0; };
struct Shared {
    std::mutex mutex;
    bool closed=false;
    std::map<std::string,Credential> access;
    std::map<std::string,Lease> users;
    Lease maintenance;
    std::map<std::string,std::shared_ptr<Job>> jobs;
    std::deque<std::shared_ptr<Job>> queue;
    wire::ImuDeviceState state;
    void Expire() {
        const auto now=Now();
        for(auto it=users.begin();it!=users.end();) it=it->second.until<=now?users.erase(it):std::next(it);
        // An in-flight device command retains exclusivity until completion.
        bool busy=false;for(const auto& [_,job]:jobs)busy|=job->reply.pending();
        if(maintenance.until<=now&&!busy)maintenance={};
        for(auto it=jobs.begin();it!=jobs.end();)it=it->second->completed&&now-it->second->completed>10000000000ULL?jobs.erase(it):std::next(it);
    }
    wire::ImuDeviceState Snapshot() {
        Expire(); auto copy=state;copy.set_server_time_monotonic_ns(Now());copy.set_users(users.size());
        copy.set_maintenance_owner(maintenance.source);
        const auto time=copy.sample().receive_time_monotonic_ns();
        copy.set_receive_fresh(!closed&&copy.error().empty()&&maintenance.token.empty()&&time&&time<=Now()&&Now()-time<100000000ULL);
        return copy;
    }
};
wire::ImuSample Sample(const ImuSample& sample,const ServiceOptions& options) {
    wire::ImuSample out;
    auto* h=out.mutable_header();h->set_robot_id(options.robot_id);h->set_source_node(options.source_node);
    h->set_source_instance(options.source_instance);h->set_sequence(sample.sequence);h->set_timestamp_unix_ns(hw::UnixTimeNs());
    out.set_sensor_id(sample.sensor_id);out.set_frame_id(sample.frame_id);out.set_sample_time_monotonic_ns(sample.sample_time_monotonic_ns);
    out.set_receive_time_monotonic_ns(sample.receive_time_monotonic_ns);out.set_sequence(sample.sequence);out.set_clock_domain(sample.clock_domain);
    out.set_reference_frame_id(sample.reference_frame_id);out.set_mounting_revision(sample.mounting_revision);
    auto vector=[](wire::Vector3* to,const Vector3& from){to->set_x(from.x);to->set_y(from.y);to->set_z(from.z);};
    vector(out.mutable_acceleration_mps2(),sample.acceleration_mps2);vector(out.mutable_angular_velocity_radps(),sample.angular_velocity_radps);
    vector(out.mutable_magnetic_field_tesla(),sample.magnetic_field_tesla);
    auto* q=out.mutable_orientation();q->set_w(sample.orientation.w);q->set_x(sample.orientation.x);q->set_y(sample.orientation.y);q->set_z(sample.orientation.z);
    out.set_temperature_celsius(sample.temperature_celsius);out.set_orientation_valid(sample.orientation_valid);
    out.set_acceleration_valid(sample.acceleration_valid);out.set_angular_velocity_valid(sample.angular_velocity_valid);
    out.set_magnetic_field_valid(sample.magnetic_field_valid);out.set_temperature_valid(sample.temperature_valid);return out;
}
wire::ImuPersistence Persistence(ImuPersistence p) {return p==ImuPersistence::kFlash?wire::IMU_PERSISTENCE_FLASH:wire::IMU_PERSISTENCE_RAM;}
ImuPersistence Persistence(wire::ImuPersistence p) {
    Require(p==wire::IMU_PERSISTENCE_RAM||p==wire::IMU_PERSISTENCE_FLASH,"explicit RAM or Flash persistence required");
    return p==wire::IMU_PERSISTENCE_FLASH?ImuPersistence::kFlash:ImuPersistence::kRam;
}
void Configuration(wire::ImuConfiguration* to,const ImuDeviceConfiguration& from) {
    to->Clear();if(from.output_rate_hz)to->set_output_rate_hz(*from.output_rate_hz);
    if(from.baud_rate)to->set_baud_rate(*from.baud_rate);if(from.output_fields)to->set_output_fields(*from.output_fields);
    to->set_host_baud_rate(from.host_baud_rate);to->set_queried_at_monotonic_ns(from.queried_at_monotonic_ns);
}
}
struct ManagedImu::Impl {
    std::unique_ptr<IImu> driver;
    ServiceOptions options;
    SensorInfo info;
    std::shared_ptr<Shared> shared=std::make_shared<Shared>();
    t::ServiceServer server;
    std::atomic<bool> stopped{true};
    mutable std::mutex driver_mutex;
    std::unique_ptr<openember::logging::Logger> audit;
    Impl(std::unique_ptr<IImu> d,ServiceOptions o):driver(std::move(d)),options(std::move(o)),info(driver?driver->Info():SensorInfo{}) {
        Require(driver&&options.context&&envelope::Id(options.robot_id)&&envelope::Id(info.sensor_id)&&!options.source_instance.empty()&&!options.host_clock_domain.empty(),"invalid managed IMU options");
        auto& state=shared->state;state.set_robot_id(options.robot_id);state.set_sensor_id(info.sensor_id);
        state.set_service_session(options.source_instance);state.set_host_clock_domain(options.host_clock_domain);
        state.set_source_node(options.source_node);state.set_device_path(options.device.device);state.set_model(options.device.model);
        state.set_maintenance_blocked_reason("product maintenance interlock initializing");
        state.set_deployment_baud_rate(options.device.baud_rate);state.mutable_configuration()->set_host_baud_rate(options.device.baud_rate);
        if(auto* configurable=dynamic_cast<IImuConfiguration*>(driver.get())) {
            const auto c=configurable->Capabilities();auto* caps=state.mutable_capabilities();caps->set_configuration_access(c.configuration_access);
            for(auto v:c.output_rates_hz)caps->add_output_rates_hz(v);for(auto v:c.baud_rates)caps->add_baud_rates(v);
            caps->set_output_fields(c.output_fields);for(auto v:c.persistence)caps->add_persistence(Persistence(v));
            caps->set_static_gyro_calibration(c.static_gyro_calibration);caps->set_profile(c.profile);
        }
        std::set<std::string> secrets;
        for(const auto& c:options.access) {
            Require(c.robot_id==options.robot_id&&envelope::Id(c.source_id)&&c.secret.size()==64&&c.secret.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos&&
                (c.role=="observe"||c.role=="use"||c.role=="maintenance")&&secrets.insert(c.secret).second&&
                shared->access.emplace(c.source_id,c).second,"invalid, duplicate or reused IMU access credential");
        }
        Require(!shared->access.empty(),"IMU access credentials required");
        auto log=openember::logging::LoggerConfig::from_build_defaults("imu_control");log.make_default=false;
        audit=std::make_unique<openember::logging::Logger>(std::move(log));
    }
    void StartServer() {
        const std::string endpoint="/sensors/imu/"+info.sensor_id+"/rpc";
        t::ServiceOptions transport;transport.name=endpoint;transport.max_payload_bytes=envelope::MaxBytes;
        auto result=options.context->Transport().CreateServiceServer(transport,[s=shared,endpoint](const t::Message& raw)->t::Result<Bytes> {
            try {
                const auto found=s->access.find(envelope::Source(raw.payload));Require(found!=s->access.end(),"unknown IMU principal");
                const auto& credential=found->second;
                const auto request=envelope::Decode<wire::ImuRequest>(envelope::Crypt(credential,endpoint,raw.payload,false));
                Require(envelope::Id(request.request_id()),"invalid request ID");
                std::lock_guard<std::mutex> lock(s->mutex);Require(!s->closed,"IMU service closed");s->Expire();
                wire::ImuReply reply;reply.set_request_id(request.request_id());
                try {
                    const auto op=request.operation();
                    if(op==wire::IMU_OPERATION_STATE)reply.set_accepted(true);
                    else {
                        Require(request.service_session()==s->state.service_session(),"IMU service session changed");
                        const auto key=credential.source_id+":"+request.request_id();
                        if(op==wire::IMU_OPERATION_RESULT) {
                            const auto job=s->jobs.find(key);Require(job!=s->jobs.end(),"unknown or expired IMU result");reply=job->second->reply;
                        } else if(const auto old=s->jobs.find(key);old!=s->jobs.end()) {
                            Require(old->second->encoded==request.SerializeAsString(),"request ID conflict");reply=old->second->reply;
                        } else {
                            const auto now=Now();Require(request.expires_time_monotonic_ns()>now&&request.expires_time_monotonic_ns()-now<=3000000000ULL,"expired or excessive IMU request deadline");
                            Require(s->jobs.size()<128,"IMU request history full");
                            const bool use=op==wire::IMU_OPERATION_ACQUIRE_USE||op==wire::IMU_OPERATION_RENEW_USE||op==wire::IMU_OPERATION_RELEASE_USE;
                            Require(credential.role==(use?"use":"maintenance"),"IMU operation permission denied");
                            if(use) {
                                Require(s->maintenance.token.empty(),"IMU maintenance in progress");
                                auto it=s->users.find(credential.source_id);
                                if(op==wire::IMU_OPERATION_ACQUIRE_USE) {
                                    Require(it==s->users.end(),"principal already holds an IMU use lease");
                                    Require(s->state.error().empty()&&s->Snapshot().receive_fresh(),"fresh IMU data unavailable");
                                    Lease lease{credential.source_id,Token(),now+2000000000ULL};s->users.emplace(credential.source_id,lease);
                                    reply.set_lease_token(lease.token);reply.set_lease_expires_time_monotonic_ns(lease.until);
                                } else {
                                    Require(it!=s->users.end()&&it->second.token==request.lease_token(),"IMU use lease invalid or expired");
                                    if(op==wire::IMU_OPERATION_RELEASE_USE)s->users.erase(it);
                                    else {it->second.until=now+2000000000ULL;reply.set_lease_token(it->second.token);reply.set_lease_expires_time_monotonic_ns(it->second.until);}
                                }
                            } else if(op==wire::IMU_OPERATION_ACQUIRE_MAINTENANCE) {
                                Require(s->users.empty(),"IMU is reserved by motion; stop the motion client first");
                                Require(s->maintenance.token.empty(),"another IMU maintenance request is active");
                                Require(s->state.maintenance_blocked_reason().empty(),s->state.maintenance_blocked_reason());
                                s->maintenance={credential.source_id,Token(),now+30000000000ULL};
                                reply.set_lease_token(s->maintenance.token);reply.set_lease_expires_time_monotonic_ns(s->maintenance.until);
                            } else {
                                Require(s->users.empty()&&s->maintenance.source==credential.source_id&&s->maintenance.token==request.lease_token()&&s->maintenance.until>now,"IMU maintenance lease invalid or expired");
                                if(op==wire::IMU_OPERATION_RELEASE_MAINTENANCE) {
                                    bool busy=false;for(const auto& [_,j]:s->jobs)busy|=j->reply.pending();Require(!busy,"device command still pending");s->maintenance={};
                                } else {
                                    Require(op>=wire::IMU_OPERATION_READ_CONFIGURATION&&op<=wire::IMU_OPERATION_RECONNECT,"unsupported IMU operation");
                                    Require(s->queue.size()<4,"IMU command queue full");
                                    reply.set_pending(true);
                                }
                            }
                            reply.set_accepted(true);
                            auto job=std::make_shared<Job>();job->source=credential.source_id;job->request=request;job->encoded=request.SerializeAsString();job->reply=reply;
                            if(reply.pending())s->queue.push_back(job);else job->completed=now;
                            s->jobs.emplace(key,job);
                        }
                    }
                } catch(const std::exception& e){reply.set_accepted(false);reply.set_error(e.what());}
                *reply.mutable_state()=s->Snapshot();
                return envelope::Crypt(credential,"reply:"+endpoint,openember::Serialize(reply),true);
            } catch(const std::exception& e){return t::Error{t::ErrorCode::kInvalidArgument,e.what()};}
        });
        Require(result.Ok(),result.Ok()?"":result.Err().message);server=std::move(result.Value());
    }
    void Replace(std::uint32_t baud) {
        Require(bool(options.factory),"IMU reconnect/probe is not supported by this assembly");
        driver->Stop();auto next=options.factory(baud);Require(bool(next),"IMU factory failed");
        Require(next->Info().sensor_id==info.sensor_id&&next->Info().frame_id==info.frame_id,"replacement IMU identity changed");
        driver=std::move(next);const auto opened=driver->Start();Require(opened.Ok(),opened.Ok()?"":opened.Err().message);
    }
    void Pump() {
        std::string gate="product maintenance interlock unavailable";
        try {if(options.maintenance_check)gate=options.maintenance_check();}catch(...){gate="product maintenance interlock failed";}
        std::shared_ptr<Job> job;
        {std::lock_guard<std::mutex> lock(shared->mutex);shared->state.set_maintenance_blocked_reason(gate);shared->Expire();
            if(shared->queue.empty())return;job=shared->queue.front();shared->queue.pop_front();}
        wire::ImuReply reply;reply.set_request_id(job->request.request_id());
        try {
            Require(!stopped,"IMU service stopping");Require(gate.empty(),gate);
            Require(Now()<job->request.expires_time_monotonic_ns(),"IMU command expired before execution");
            {std::lock_guard<std::mutex> lock(shared->mutex);Require(shared->maintenance.token==job->request.lease_token()&&shared->maintenance.until>Now()&&shared->users.empty(),"IMU maintenance authorization expired");}
            const auto op=job->request.operation();
            audit->warn("IMU maintenance source="+job->source+" request="+job->request.request_id()+" operation="+wire::ImuOperation_Name(op));
            auto* config=dynamic_cast<IImuConfiguration*>(driver.get());
            if(op==wire::IMU_OPERATION_READ_CONFIGURATION) {
                Require(config,"IMU configuration unsupported");const auto r=config->ReadConfiguration();Require(r.Ok(),r.Ok()?"":r.Err().message);
                std::lock_guard<std::mutex> lock(shared->mutex);Configuration(shared->state.mutable_configuration(),r.Value());
            } else if(op==wire::IMU_OPERATION_APPLY_CONFIGURATION) {
                Require(config,"IMU configuration unsupported");const auto& p=job->request.patch();ImuConfigurationPatch patch;
                if(p.output_rate_hz_presence_case()==wire::ImuConfigurationPatch::kOutputRateHz)patch.output_rate_hz=p.output_rate_hz();if(p.baud_rate_presence_case()==wire::ImuConfigurationPatch::kBaudRate)patch.baud_rate=p.baud_rate();if(p.output_fields_presence_case()==wire::ImuConfigurationPatch::kOutputFields)patch.output_fields=p.output_fields();
                patch.persistence=Persistence(p.persistence());const auto r=config->ApplyConfiguration(patch);Require(r.Ok(),r.Ok()?"":r.Err().message);
                for(const auto& s:r.Value().settings) {auto* setting=reply.add_settings();setting->set_setting(s.setting);setting->set_state(s.state==ImuSettingState::kApplied?"applied":s.state==ImuSettingState::kRejected?"rejected":"unconfirmed");setting->set_message(s.message);}
                std::lock_guard<std::mutex> lock(shared->mutex);shared->state.clear_configuration();
                if(r.Value().observed)Configuration(shared->state.mutable_configuration(),*r.Value().observed);
            } else if(op==wire::IMU_OPERATION_CALIBRATE_STATIC_GYRO) {
                auto* calibration=dynamic_cast<IImuCalibration*>(driver.get());Require(calibration,"static gyro calibration unsupported");
                const auto r=calibration->CalibrateStaticGyro({Persistence(job->request.calibration_persistence())});Require(r.Ok(),r.Ok()?"":r.Err().message);
                auto* s=reply.add_settings();s->set_setting("static_gyro_calibration");s->set_state(r.Value().state==ImuCalibrationState::kCompleted?"completed":r.Value().state==ImuCalibrationState::kAccepted?"accepted":r.Value().state==ImuCalibrationState::kRejected?"rejected":"unconfirmed");s->set_message(r.Value().message);
            } else if(op==wire::IMU_OPERATION_RECONNECT) {
                Replace(options.device.baud_rate);std::lock_guard<std::mutex> lock(shared->mutex);shared->state.clear_configuration();shared->state.mutable_configuration()->set_host_baud_rate(options.device.baud_rate);
            } else if(op==wire::IMU_OPERATION_PROBE_BAUD) {
                Require(config,"IMU configuration unsupported");const auto rates=config->Capabilities().baud_rates;
                for(auto baud:rates) {
                    if(Now()>=job->request.expires_time_monotonic_ns()||stopped)break;
                    try {Replace(baud);auto* c=dynamic_cast<IImuConfiguration*>(driver.get());if(!c)continue;const auto r=c->ReadConfiguration();if(r.Ok()){reply.set_detected_baud_rate(baud);break;}}catch(...){}
                }
                // Probe never changes deployment parameters or device Flash.
                Replace(options.device.baud_rate);Require(reply.detected_baud_rate()!=0,"no baud detected within bounded probe window");
            }
            reply.set_accepted(true);
        } catch(const std::exception& e){reply.set_error(e.what());audit->warn("IMU maintenance rejected/unconfirmed: "+std::string(e.what()));}
        std::lock_guard<std::mutex> lock(shared->mutex);job->reply=std::move(reply);job->completed=Now();
        shared->state.clear_sample();shared->state.clear_raw_sample();
    }
};
ManagedImu::ManagedImu(std::unique_ptr<IImu> d,ServiceOptions o):impl_(std::make_unique<Impl>(std::move(d),std::move(o))){}
ManagedImu::~ManagedImu(){Stop();}
const SensorInfo& ManagedImu::Info()const{return impl_->info;}
hw::DeviceStatus ManagedImu::Status()const{std::lock_guard<std::mutex> lock(impl_->driver_mutex);return impl_->driver->Status();}
hw::Result<void> ManagedImu::Start(){
    auto& p=*impl_;try{
        if(!p.stopped)return hw::Result<void>::Success();
        auto result=p.driver->Start();if(!result.Ok())return result;
        {std::lock_guard<std::mutex> lock(p.shared->mutex);p.shared->closed=false;p.shared->state.clear_sample();p.shared->state.clear_raw_sample();p.shared->state.clear_error();}
        p.stopped=false;p.StartServer();return hw::Result<void>::Success();}
    catch(const std::exception& e){Stop();return hw::Result<void>::Failure(hw::MakeError(hw::ErrorCode::kIoError,e.what()));}
}
void ManagedImu::Stop()noexcept{
    auto& p=*impl_;p.stopped=true;
    {std::lock_guard<std::mutex> lock(p.shared->mutex);p.shared->closed=true;p.shared->users.clear();p.shared->maintenance={};p.shared->queue.clear();
        for(auto& [_,job]:p.shared->jobs)if(job->reply.pending()){job->reply.set_pending(false);job->reply.set_accepted(false);job->reply.set_error("IMU service stopped; execution outcome unconfirmed");job->completed=Now();}}
    p.server={};std::lock_guard<std::mutex> lock(p.driver_mutex);p.driver->Stop();
}
hw::Result<ImuSample> ManagedImu::ReadSample(std::chrono::milliseconds timeout){
    auto& p=*impl_;std::lock_guard<std::mutex> driver_lock(p.driver_mutex);
    if(p.stopped)return hw::Result<ImuSample>::Failure(hw::MakeError(hw::ErrorCode::kNotRunning,"IMU service stopped"));
    p.Pump();auto result=p.driver->ReadSample(std::min(timeout,std::chrono::milliseconds(10)));
    if(result.Ok()) {
        const auto installed=ApplyImuInstallation(result.Value(),p.options.device.installation);
        std::lock_guard<std::mutex> lock(p.shared->mutex);
        *p.shared->state.mutable_raw_sample()=Sample(result.Value(),p.options);
        if(installed.Ok()){*p.shared->state.mutable_sample()=Sample(installed.Value(),p.options);p.shared->state.clear_error();}
        else {p.shared->state.clear_sample();p.shared->state.set_error(installed.Err().message);}
    }
    if(!result.Ok() && result.Err().code!=hw::ErrorCode::kTimeout) {
        {std::lock_guard<std::mutex> lock(p.shared->mutex);p.shared->state.set_error(result.Err().message);}
        // Keep the owner available for explicit reconnect; never replay data.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return hw::Result<ImuSample>::Failure(hw::MakeError(hw::ErrorCode::kTimeout,result.Err().message));
    }
    return result;
}
} // namespace openember::sensor::link
