#include "openember/config/yaml.hpp"
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>

namespace c = openember::config;
void Check(bool condition) { if (!condition) throw std::runtime_error("configuration expectation failed"); }
void Reject(const std::function<void()>& action, c::ErrorCode code) {
    try { action(); } catch (const c::ConfigError& error) { Check(error.Code() == code); return; }
    throw std::runtime_error("configuration unexpectedly accepted");
}
int main(int argc, char** argv) {
    try {
        Check(argc == 3); const std::string name = argv[1];
        const auto root = c::ParseYaml("schema_version: 1\ncontroller:\n  kp: 80.0\n  enabled: true\n  targets: [0.0, 0.8, -1.5]\n", "robot.yaml");
        const auto controller = root.At("controller");
        if (name == "typed") {
            Check(root.Get<int>("schema_version") == 1 && controller.Get<bool>("enabled"));
            Check(controller.Get<double>("kp") == 80 && controller.Get<float>("kp") == 80);
            Check(controller.Get<std::string>("kp") == "80.0");
            Check(controller.At("targets").As<std::vector<double>>() == std::vector<double>({0, .8, -1.5}));
            const auto boundaries = c::ParseYaml("lo: -9223372036854775808\nhi: 18446744073709551615\n");
            Check(boundaries.Get<std::int64_t>("lo") == std::numeric_limits<std::int64_t>::min());
            Check(boundaries.Get<std::uint64_t>("hi") == std::numeric_limits<std::uint64_t>::max());
            Reject([&] { boundaries.Get<unsigned>("lo"); }, c::ErrorCode::kType);
            Reject([&] { boundaries.Get<std::int64_t>("hi"); }, c::ErrorCode::kType);
            Check(controller.At("kp").AsInRange<double>(0, 100) == 80);
            Reject([&] { controller.At("kp").AsInRange<double>(0, 20); }, c::ErrorCode::kInvalidValue);
        } else if (name == "defaults") {
            Check(controller.GetOr<int>("missing", 7) == 7 && controller.GetOr<double>("kp", 7) == 80);
            const auto values = c::ParseYaml("null_value: null\nwrong: []\n");
            Reject([&] { values.GetOr<int>("null_value", 7); }, c::ErrorCode::kType);
            Reject([&] { values.GetOr<int>("wrong", 7); }, c::ErrorCode::kType);
            Reject([&] { controller.At("missing"); }, c::ErrorCode::kMissing);
        } else if (name == "structure") {
            root.CheckKeys({"schema_version", "controller"});
            Check(controller.Size() == 3 && controller.Keys().size() == 3);
            Check(!controller.Find("absent") && controller.At("targets").At(1).As<double>() == .8);
            Reject([&] { controller.CheckKeys({"enabled"}); }, c::ErrorCode::kUnknownKey);
            Reject([&] { controller.At("targets").At(3); }, c::ErrorCode::kMissing);
            Reject([&] { controller.At("kp").Size(); }, c::ErrorCode::kType);
            Reject([&] { controller.As<std::vector<int>>(); }, c::ErrorCode::kType);
        } else if (name == "errors") {
            try { controller.At("enabled").As<double>(); Check(false); }
            catch (const c::ConfigError& error) {
                Check(error.Source() == "robot.yaml" && error.Path() == "$[\"controller\"][\"enabled\"]");
                Check(error.Line() == 4 && error.Column() == 12);
                Check(std::string(error.what()).find("robot.yaml:4:12") != std::string::npos);
            }
            for (const auto& scalar : {"nan", ".nan", ".inf", "inf", "1e9999", "true", "\"\"", "\"1.0 junk\""})
                Reject([&] { c::ParseYaml(std::string("x: ") + scalar).Get<double>("x"); }, c::ErrorCode::kType);
            for (const auto& scalar : {"1.5", "0x20", "true", "9223372036854775808"})
                Reject([&] { c::ParseYaml(std::string("x: ") + scalar).Get<std::int64_t>("x"); }, c::ErrorCode::kType);
            Reject([&] { c::ParseYaml("x: 256").Get<std::uint8_t>("x"); }, c::ErrorCode::kInvalidValue);
            Reject([&] { c::ParseYaml("x: yes").Get<bool>("x"); }, c::ErrorCode::kType);
            Reject([&] { c::ParseYaml("x: 1e100").Get<float>("x"); }, c::ErrorCode::kInvalidValue);
        } else if (name == "duplicate") {
            Reject([] { c::ParseYaml("outer: {gain: 1, gain: 2}"); }, c::ErrorCode::kDuplicateKey);
            Reject([] { c::ParseYaml("1: a\n\"1\": b"); }, c::ErrorCode::kDuplicateKey);
        } else if (name == "syntax") {
            Reject([] { c::ParseYaml("x: [1,"); }, c::ErrorCode::kParse);
            Reject([] { c::ParseYaml(""); }, c::ErrorCode::kParse);
            Reject([] { c::ParseYaml("---\nx: 1\n---\nx: 2"); }, c::ErrorCode::kParse);
            Reject([] { c::ParseYaml("? [a, b]\n: 1"); }, c::ErrorCode::kParse);
        } else if (name == "limits") {
            Reject([] { c::ParseYaml("x: 1", "size", {3, 32, 65536}); }, c::ErrorCode::kLimit);
            Reject([] { c::ParseYaml("x: {y: {z: 1}}", "depth", {1024, 2, 65536}); }, c::ErrorCode::kLimit);
            Reject([] { c::ParseYaml("[1, 2, 3]", "nodes", {1024, 32, 3}); }, c::ErrorCode::kLimit);
            Check(c::ParseYaml("[1, 2]", "exact", {6, 2, 3}).Size() == 2);
        } else if (name == "aliases") {
            const auto aliases = c::ParseYaml("a: &g [1, 2]\nb: *g");
            Check(aliases.At("b").As<std::vector<int>>() == std::vector<int>({1, 2}));
            Reject([] { c::ParseYaml("a: &cycle {again: *cycle}"); }, c::ErrorCode::kLimit);
            Reject([] { c::ParseYaml("a: &g [1, 2]\nb: [*g, *g]", "aliases", {1024, 32, 8}); }, c::ErrorCode::kLimit);
            Reject([] { c::ParseYaml("a: &g {x: 1}\nb: {<<: *g}"); }, c::ErrorCode::kParse);
        } else if (name == "tags") {
            Reject([] { c::ParseYaml("x: !include other.yaml"); }, c::ErrorCode::kParse);
            Reject([] { c::ParseYaml("x: !!str 42"); }, c::ErrorCode::kParse);
            Check(c::ParseYaml("x: '${HOME}'").Get<std::string>("x") == "${HOME}");
        } else if (name == "files") {
            const auto path = std::filesystem::path(argv[2]) / "config-file-test.yaml";
            { std::ofstream out(path); out << "gain: 80"; }
            Check(c::LoadYamlFile(path.string()).Get<int>("gain") == 80);
            Reject([&] { c::LoadYamlFile(path.string(), {2, 32, 65536}); }, c::ErrorCode::kLimit);
            std::filesystem::remove(path);
            Reject([&] { c::LoadYamlFile(path.string()); }, c::ErrorCode::kIo);
        } else if (name == "lifetime") {
            const auto retained = [] { return c::ParseYaml("x: {y: [1, 2]}").At("x").At("y"); }();
            Check(retained.At(1).As<int>() == 2);
            const auto other = c::ParseYaml("x: 3");
            Check(retained.At(0).As<int>() == 1 && other.Get<int>("x") == 3);
        } else if (name == "concurrency") {
            std::atomic<bool> good{true}; std::vector<std::thread> readers;
            for (int i = 0; i < 4; ++i) readers.emplace_back([&] {
                for (int j = 0; j < 1000; ++j) if (controller.Get<double>("kp") != 80) good = false;
            });
            for (auto& thread : readers) thread.join();
            Check(good);
        } else throw std::runtime_error("unknown test");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
