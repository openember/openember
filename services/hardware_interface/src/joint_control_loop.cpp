#include "openember/services/hardware_interface/joint_control_loop.hpp"

#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <variant>

namespace openember::services::hardware_interface {
namespace a = actuator_control;
namespace d = a::domain;
namespace w = a::wire;
namespace {
using Reply = a::Result<w::ControlResult>;
Reply Unavailable(const char* why) {
    return Reply::Failure(hardware::MakeError(hardware::ErrorCode::kNotRunning, why));
}
constexpr std::size_t kNormalCapacity = 32, kPriorityCapacity = 8, kBytes = 16384;
struct ControlWork {
    w::ControlRequest request;
    d::Permission permission;
    std::promise<Reply> promise;
};
struct CommandWork { w::JointCommand command; std::promise<d::Decision> promise; };
using Work = std::variant<ControlWork, CommandWork>;
}

struct JointControlLoop::Impl {
    JointControlLoopOptions options;
    std::atomic<bool> stop{false}, running{false};
    bool started = false;
    std::thread worker;
    mutable std::mutex mutex;
    std::mutex lifecycle;
    std::condition_variable wake;
    std::deque<Work> normal;
    std::deque<ControlWork> priority;
    d::Status status;
    std::optional<w::ControllerFeedback> latest;
    std::string error;

    explicit Impl(JointControlLoopOptions value) : options(std::move(value)) {
        if (!d::ValidateCapabilities(options.capabilities).Ok() || !options.backend || !options.read ||
            !options.tokens || !options.clock || options.period.count() <= 0 ||
            static_cast<std::uint64_t>(options.period.count()) >= options.capabilities.command_timeout_ns)
            throw std::invalid_argument("invalid joint control loop options or period");
    }
    bool StopContext(const d::Request& request) const {
        const auto& c = request.context; const auto& expected = options.capabilities;
        const auto now = options.clock();
        return c.robot_id == expected.robot_id && c.controller_id == expected.controller_id &&
            c.controller_session == expected.controller_session && c.model_revision == expected.model_revision &&
            c.clock_domain == expected.clock_domain && c.issued_time_ns <= now && now < c.expires_time_ns &&
            c.expires_time_ns - c.issued_time_ns <= expected.max_command_duration_ns;
    }
    void Store(JointControlSession& session, std::optional<d::FeedbackFrame>& frame, std::uint64_t& sequence) {
        std::optional<w::ControllerFeedback> publication;
        if (frame) {
            if (sequence == std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("feedback sequence exhausted");
            frame->sequence = ++sequence;
            frame->publish_time_ns = options.clock();
            session.Tick(frame->publish_time_ns);
            auto encoded = session.Feedback(*frame);
            if (!encoded.Ok()) throw std::runtime_error(encoded.Err().message);
            publication = std::move(encoded.Value());
        }
        auto snapshot = session.Status();
        std::lock_guard<std::mutex> lock(mutex);
        status = std::move(snapshot);
        if (publication) latest = std::move(publication);
    }
    void DrainPriority(JointControlSession& session) {
        // Finite per wake; each backend call must be bounded.
        for (std::size_t i = 0; i < kPriorityCapacity && !stop.load(); ++i) {
            std::optional<ControlWork> work;
            { std::lock_guard<std::mutex> lock(mutex);
              if (priority.empty()) break;
              work.emplace(std::move(priority.front())); priority.pop_front(); }
            auto result = session.Control(work->request, options.clock(), work->permission);
            if (result.Ok() && result.Value().code() != w::CONTROL_RESULT_CODE_REJECTED) {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& item : normal) {
                    if (auto* control = std::get_if<ControlWork>(&item))
                        control->promise.set_value(Unavailable("cancelled by priority stop"));
                    else std::get<CommandWork>(item).promise.set_value({d::Reason::kInvalidState});
                }
                normal.clear();
            }
            work->promise.set_value(std::move(result));
        }
    }
    void FinishQueues() {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& item : priority) item.promise.set_value(Unavailable("control loop stopped before dispatch"));
        priority.clear();
        for (auto& item : normal) {
            if (auto* control = std::get_if<ControlWork>(&item))
                control->promise.set_value(Unavailable("control loop stopped before dispatch"));
            else std::get<CommandWork>(item).promise.set_value({d::Reason::kInvalidState});
        }
        normal.clear();
    }
    void Run(std::promise<bool> ready) noexcept {
        std::unique_ptr<JointControlSession> session;
        std::optional<d::FeedbackFrame> frame;
        std::uint64_t sequence = 0;
        bool announced = false;
        try {
            session = std::make_unique<JointControlSession>(options.capabilities, options.backend(), options.tokens);
            if (session->Status().backend_failure) throw std::runtime_error("initial backend disable failed");
            { std::lock_guard<std::mutex> lock(mutex); status = session->Status(); }
            running.store(!stop.load());
            ready.set_value(!stop.load()); announced = true;
            auto deadline = std::chrono::steady_clock::now();
            while (!stop.load()) {
                { std::unique_lock<std::mutex> lock(mutex);
                  wake.wait_until(lock, deadline, [&] { return stop.load() || !priority.empty(); }); }
                if (stop.load()) break;
                DrainPriority(*session);
                if (stop.load()) break;
                if (std::chrono::steady_clock::now() < deadline) { Store(*session, frame, sequence); continue; }
                session->Tick(options.clock());
                frame = options.read();
                if (options.capabilities.simulation && session->Status().paused != frame->paused)
                    session->SetPaused(frame->paused, options.clock());
                frame->sequence = sequence + 1;
                frame->publish_time_ns = options.clock();
                session->Tick(frame->publish_time_ns);
                auto checked = session->Feedback(*frame);
                if (!checked.Ok()) throw std::runtime_error(checked.Err().message);
                // A fixed batch bounds work per cycle. Priority is checked between items.
                for (int i = 0; i < 4 && !stop.load(); ++i) {
                    DrainPriority(*session);
                    if (stop.load()) break;
                    std::optional<Work> work;
                    { std::lock_guard<std::mutex> lock(mutex);
                      if (normal.empty()) break;
                      work.emplace(std::move(normal.front())); normal.pop_front(); }
                    if (auto* control = std::get_if<ControlWork>(&*work))
                        control->promise.set_value(session->Control(control->request, options.clock(), control->permission));
                    else {
                        auto& command = std::get<CommandWork>(*work);
                        command.promise.set_value(session->Command(command.command, options.clock()));
                    }
                }
                // Catch a stop arriving during the batch before applying a target.
                DrainPriority(*session);
                if (stop.load()) break;
                const auto now = options.clock();
                session->Tick(now);
                bool feedback_ok = true;
                for (int i = 0; i < checked.Value().joints_size(); ++i) {
                    const auto& joint = checked.Value().joints(i);
                    const auto found = std::find_if(options.capabilities.joints.begin(), options.capabilities.joints.end(),
                        [&](const auto& item) { return item.joint_name == joint.joint_name(); });
                    feedback_ok = feedback_ok && joint.position_valid() && joint.velocity_valid() &&
                        d::SampleFresh(joint.sample_sequence(), joint.receive_time_ns(), joint.receive_fresh(),
                                       found->max_feedback_age_ns, now);
                }
                if (session->Status().state == d::State::kArmed && !feedback_ok)
                    session->TripFault(d::Reason::kExpired, now);
                if (session->Status().state == d::State::kArmed && session->Status().last_accepted_command_sequence)
                    session->Apply(options.clock());
                Store(*session, frame, sequence);
                deadline += options.period;
                if (deadline <= std::chrono::steady_clock::now()) deadline = std::chrono::steady_clock::now() + options.period;
            }
        } catch (const std::exception& exception) {
            std::lock_guard<std::mutex> lock(mutex); error = exception.what();
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex); error = "unexpected joint control worker exception";
        }
        stop.store(true);
        if (session) {
            session->Stop();
            try { Store(*session, frame, sequence); } catch (...) {
                std::lock_guard<std::mutex> lock(mutex); status = session->Status(); latest.reset();
            }
        } else { std::lock_guard<std::mutex> lock(mutex); status.state = d::State::kStopped; }
        session.reset(); // Stop and backend destruction also stay on the worker.
        running.store(false);
        FinishQueues();
        if (!announced) ready.set_value(false);
    }
};

JointControlLoop::JointControlLoop(JointControlLoopOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
JointControlLoop::~JointControlLoop() { Stop(); }
bool JointControlLoop::Start() {
    std::lock_guard<std::mutex> lock(impl_->lifecycle);
    if (impl_->started || impl_->stop.load()) return false;
    impl_->started = true;
    std::promise<bool> ready; auto future = ready.get_future();
    impl_->worker = std::thread([this, ready = std::move(ready)]() mutable { impl_->Run(std::move(ready)); });
    return future.get();
}
void JointControlLoop::RequestStop() noexcept { impl_->stop.store(true); impl_->wake.notify_all(); }
void JointControlLoop::Stop() noexcept {
    RequestStop();
    std::lock_guard<std::mutex> lock(impl_->lifecycle);
    if (impl_->worker.joinable()) impl_->worker.join();
}
std::future<Reply> JointControlLoop::Control(const w::ControlRequest& request, d::Permission permission) {
    ControlWork work{{}, permission, {}}; auto future = work.promise.get_future();
    auto parsed = request.ByteSizeLong() <= kBytes ? a::FromProto(request) :
        a::Result<d::Request>::Failure(hardware::MakeError(hardware::ErrorCode::kInvalidData, "control message exceeds byte limit"));
    if (!parsed.Ok()) { work.promise.set_value(Reply::Failure(parsed.Err())); return future; }
    work.request = request;
    const bool urgent = request.operation() == w::CONTROL_OPERATION_DISARM || request.operation() == w::CONTROL_OPERATION_EMERGENCY_STOP;
    bool stop_context_ok = !urgent;
    if (urgent) {
        try { stop_context_ok = permission.stop && impl_->StopContext(parsed.Value()); }
        catch (...) {
            { std::lock_guard<std::mutex> lock(impl_->mutex); impl_->error = "control clock callback failed"; }
            RequestStop(); work.promise.set_value(Unavailable("control clock callback failed")); return future;
        }
    }
    if (!stop_context_ok) {
        work.promise.set_value(Reply::Failure(hardware::MakeError(hardware::ErrorCode::kSafetyViolation, "invalid priority stop context or permission")));
        return future;
    }
    { std::lock_guard<std::mutex> lock(impl_->mutex);
      if (!impl_->running.load() || impl_->stop.load()) work.promise.set_value(Unavailable("control loop is not running"));
      else if (urgent) {
          if (impl_->priority.size() >= kPriorityCapacity) {
              impl_->stop.store(true); work.promise.set_value(Unavailable("priority capacity exhausted; terminating session"));
          } else impl_->priority.push_back(std::move(work));
      } else if (impl_->normal.size() >= kNormalCapacity)
          work.promise.set_value(Reply::Failure(hardware::MakeError(hardware::ErrorCode::kTimeout, "control queue full")));
      else impl_->normal.emplace_back(std::move(work)); }
    impl_->wake.notify_all();
    return future;
}
std::future<d::Decision> JointControlLoop::Command(const w::JointCommand& command) {
    CommandWork work{{}, {}}; auto future = work.promise.get_future();
    if (command.ByteSizeLong() > kBytes || !a::FromProto(command).Ok()) {
        work.promise.set_value({d::Reason::kInvalidArgument}); return future;
    }
    work.command = command;
    { std::lock_guard<std::mutex> lock(impl_->mutex);
      if (!impl_->running.load() || impl_->stop.load()) work.promise.set_value({d::Reason::kInvalidState});
      else if (impl_->normal.size() >= kNormalCapacity) work.promise.set_value({d::Reason::kBusy});
      else impl_->normal.emplace_back(std::move(work)); }
    return future;
}
d::Status JointControlLoop::Status() const { std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->status; }
std::optional<w::ControllerFeedback> JointControlLoop::Latest() const { std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->latest; }
std::string JointControlLoop::Error() const { std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->error; }
bool JointControlLoop::Running() const noexcept { return impl_->running.load(); }
}  // namespace openember::services::hardware_interface
