#include "envelope.hpp"
#include "openember/transport/service_client.hpp"
#include <chrono>
namespace openember::sensor::link {
struct Client::Impl {
    Credential credential;
    std::string endpoint,session;
    std::shared_ptr<openember::Context> context;
    openember::transport::ServiceClient service;
    std::uint64_t server_time=0;
    std::chrono::steady_clock::time_point received;
    Impl(const std::string& locator,Credential c,const std::string& sensor):credential(std::move(c)),endpoint("/sensors/imu/"+sensor+"/rpc") {
        RequireLoopback(locator);if(!envelope::Id(sensor))throw std::invalid_argument("invalid IMU ID");
        openember::RuntimeOptions options;options.robot_id=credential.robot_id;options.link.profile=openember::link::Profile::kClient;options.link.connect=locator;
        context=std::make_shared<openember::Context>(options);if(!context->Init())throw std::runtime_error("IMU Link client failed");
        openember::transport::ServiceOptions service_options;service_options.name=endpoint;service_options.max_payload_bytes=envelope::MaxBytes;
        auto result=context->Transport().CreateServiceClient(service_options);if(!result.Ok())throw std::runtime_error(result.Err().message);
        service=std::move(result.Value());
    }
};
Client::Client(const std::string& locator,Credential c,std::string sensor):impl_(std::make_unique<Impl>(locator,std::move(c),sensor)){}
Client::~Client()=default;
wire::ImuReply Client::Call(const wire::ImuRequest& request) {
    auto& p=*impl_;const auto payload=envelope::Crypt(p.credential,p.endpoint,openember::Serialize(request),true);
    auto response=p.service.Call(payload,std::chrono::milliseconds(500));
    if(!response.Ok())throw std::runtime_error("IMU RPC unavailable; commands are not automatically retried");
    auto reply=envelope::Decode<wire::ImuReply>(envelope::Crypt(p.credential,"reply:"+p.endpoint,response.Value(),false));
    if(reply.request_id()!=request.request_id()||reply.state().robot_id()!=p.credential.robot_id||reply.state().service_session().empty()||!reply.state().server_time_monotonic_ns())
        throw std::runtime_error("IMU authenticated response mismatch");
    p.session=reply.state().service_session();p.server_time=reply.state().server_time_monotonic_ns();p.received=std::chrono::steady_clock::now();
    return reply;
}
wire::ImuRequest Client::Request(wire::ImuOperation op,const std::string& lease) {
    auto& p=*impl_;wire::ImuRequest request;request.set_request_id(Token().substr(0,32));request.set_service_session(p.session);request.set_operation(op);request.set_lease_token(lease);
    if(p.server_time)request.set_expires_time_monotonic_ns(p.server_time+std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-p.received).count()+3000000000ULL);
    return request;
}
wire::ImuReply Client::State(){return Call(Request(wire::IMU_OPERATION_STATE));}
wire::ImuReply Client::Result(const std::string& id){auto r=Request(wire::IMU_OPERATION_RESULT);r.set_request_id(id);return Call(r);}
} // namespace openember::sensor::link
