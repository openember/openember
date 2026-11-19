#include "openember/alerts/manager.hpp"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace openember::alerts;
using namespace std::chrono_literals;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
Event Make(std::uint64_t sequence = 1, Transition t = Transition::Raised) {
    Event e; e.key = {"motion", "P05W", "session-1", "tilt"};
    e.sequence = sequence; e.time_ns = sequence * 100; e.transition = t;
    e.clock_domain = "simulation"; e.value = .8; e.threshold = .7; e.clear_threshold = .6;
    e.unit = "rad"; e.context = "locomotion"; return e;
}
Options Config(const std::string& name, const std::string& dir) {
    Options o; o.logging.name = "alerts-test-" + name; o.logging.file_dir = dir;
    o.logging.enable_stdout = o.logging.enable_syslog = o.logging.enable_topic = false;
    o.logging.enable_file = true; o.logging.pattern = "[%l] %v";
    // Alert logging must still include warnings under an error-only process profile.
    o.logging.log_level = o.logging.topic_level = "error"; return o;
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("case and output directory required");
        const std::string name = argv[1], dir = argv[2];
        auto options = Config(name, dir);
        if (name == "lifecycle") {
            Manager m(options);
            for (auto t : {Transition::Raised, Transition::Reminder, Transition::Unknown, Transition::Cleared}) {
                auto e = Make(m.Stats().accepted + 1, t);
                if (t == Transition::Unknown) e.valid = false;
                Check(m.Publish(e) == PublishResult::Accepted, "lifecycle rejected");
                Check(m.Snapshot().at(0).transition == t, "wrong snapshot");
            }
            auto newer = Make(); newer.key.session = "session-2";
            Check(m.Publish(newer) == PublishResult::Accepted, "new session rejected");
            m.ForgetSession("motion", "session-1");
            Check(m.Snapshot().size() == 1 && m.Snapshot()[0].key.session == "session-2", "session cleanup");
            m.Stop(); m.Stop();
            Check(m.Stats().delivered == 5, "stop did not drain");
            Check(m.Publish(Make(9)) == PublishResult::Stopped, "publish after stop");
        } else if (name == "validation") {
            Manager m(options); auto e = Make();
            Check(m.Publish(e) == PublishResult::Accepted, "initial");
            Check(m.Publish(e) == PublishResult::Stale, "replay accepted");
            e = Make(2); e.time_ns = 0;
            Check(m.Publish(e) == PublishResult::Stale, "backward time accepted");
            e = Make(2); e.clock_domain = "host";
            Check(m.Publish(e) == PublishResult::Stale, "clock change accepted");
            e = Make(2); e.value = std::numeric_limits<double>::quiet_NaN();
            Check(m.Publish(e) == PublishResult::Invalid, "NaN accepted");
            e = Make(2); e.message = "log\ninjection";
            Check(m.Publish(e) == PublishResult::Invalid, "control characters accepted");
            e = Make(2); e.key.code.assign(257, 'a');
            Check(m.Publish(e) == PublishResult::Invalid, "unbounded text accepted");
            e = Make(2); e.valid = false;
            Check(m.Publish(e) == PublishResult::Invalid, "invalid observation asserted active");
            Check(m.Snapshot()[0].sequence == 1, "rejections changed state");
            m.Stop(); Check(m.Stats().rejected == 7, "wrong rejection count");
        } else if (name == "capacity") {
            options.state_capacity = 1; Manager m(options);
            Check(m.Publish(Make()) == PublishResult::Accepted, "initial");
            auto second = Make(); second.key.code = "height";
            Check(m.Publish(second) == PublishResult::Full, "registry exceeded bound");
            m.ForgetSession("motion", "session-1");
            Check(m.Publish(second) == PublishResult::Accepted, "cleanup did not free capacity");
            m.Stop(); Check(m.Stats().delivered == 2, "pending event lost during cleanup");
            options.queue_capacity = 0; bool bad = false;
            try { Manager invalid(options); } catch (const std::invalid_argument&) { bad = true; }
            Check(bad, "zero capacity accepted");
        } else if (name == "async") {
            std::mutex mutex; std::condition_variable cv; bool entered = false, release = false;
            options.queue_capacity = 1;
            options.observer = [&](const Event&) {
                std::unique_lock<std::mutex> lock(mutex); entered = true; cv.notify_all();
                cv.wait(lock, [&] { return release; });
                throw std::runtime_error("observer failure");
            };
            Manager m(options); m.Publish(Make());
            bool started;
            { std::unique_lock<std::mutex> lock(mutex); started = cv.wait_for(lock, 2s, [&] { return entered; }); }
            const auto start = std::chrono::steady_clock::now();
            const auto accepted = m.Publish(Make(2, Transition::Reminder));
            const auto full = m.Publish(Make(3, Transition::Cleared));
            const auto elapsed = std::chrono::steady_clock::now() - start;
            { std::lock_guard<std::mutex> lock(mutex); release = true; cv.notify_all(); }
            m.Stop();
            Check(started && accepted == PublishResult::Accepted && full == PublishResult::Full, "bounded queue");
            Check(elapsed < 100ms, "producer blocked on observer");
            Check(m.Snapshot()[0].sequence == 2 && m.Stats().rejected == 1, "full event mutated state");
            Check(m.Stats().delivered == 2 && m.Stats().dispatch_errors == 2, "exception killed dispatcher");
        } else if (name == "logging") {
            const auto file = std::filesystem::path(dir) / "alerts-test-logging-1.log";
            std::filesystem::remove(file);
            {
                Manager m(options); auto e = Make();
                e.message = "quoted \"context\"";
                m.Publish(e); e = Make(2, Transition::Cleared); m.Publish(e);
                e = Make(3); e.severity = Severity::Error; m.Publish(e); m.Stop();
            }
            std::ifstream input(file); std::string line; int warnings = 0, errors = 0;
            bool cleared = false, metadata = false;
            while (std::getline(input, line)) {
                warnings += line.find("[warning]") != std::string::npos;
                errors += line.find("[error]") != std::string::npos;
                cleared |= line.find("edge=CLEARED") != std::string::npos;
                metadata |= line.find("clock_domain=\"simulation\"") != std::string::npos &&
                            line.find("unit=\"rad\"") != std::string::npos && line.find("session=\"session-1\"") != std::string::npos;
            }
            Check(warnings == 2 && errors == 1 && cleared && metadata, "warning/error log output or metadata missing (requires spdlog)");
        } else throw std::runtime_error("unknown case");
        std::cout << "alerts " << name << " OK\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
