#include "openember/inference/model.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace inf = openember::inference;
void Check(bool condition) { if (!condition) throw std::runtime_error("inference expectation failed"); }
template<class F> void Reject(F&& call) { bool rejected = false; try { call(); } catch (const std::exception&) { rejected = true; } Check(rejected); }
int main(int argc, char** argv) {
    try {
        Check(argc == 3); const std::string root = argv[1], name = argv[2];
        if (name == "dtype") { Reject([&] { inf::OnnxModel model(root + "/integer.onnx"); }); return 0; }
        if (name == "missing") { Reject([&] { inf::OnnxModel model(root + "/absent.onnx"); }); return 0; }
        inf::OnnxModel model(root + (name == "dynamic" ? "/dynamic.onnx" : name == "output" ? "/nonfinite.onnx" : "/identity.onnx"));
        Check(model.Inputs().size() == 1 && model.Inputs()[0].name == "x");
        if (name == "shape") {
            Reject([&] { model.Prepare({{"x", {1, 4}}}, {{"y", {1, 3}}}); });
            Reject([&] { model.Prepare({{"wrong", {1, 3}}}, {{"y", {1, 3}}}); });
            Reject([&] { model.Prepare({{"x", {0, 3}}}, {{"y", {1, 3}}}); });
            inf::OnnxModel bounded(root + "/identity.onnx", {1, 2});
            Reject([&] { bounded.Prepare({{"x", {1, 3}}}, {{"y", {1, 3}}}); }); return 0;
        }
        const std::int64_t batch = name == "dynamic" ? 2 : 1;
        model.Prepare({{"x", {batch, 3}}}, {{"y", {batch, 3}}});
        Reject([&] { model.Prepare({{"x", {batch, 3}}}, {{"y", {batch, 3}}}); });
        Reject([&] { model.Output("y"); });
        auto input = model.Input("x");
        for (std::size_t i = 0; i < input.size; ++i) input.data[i] = static_cast<float>(i + 1);
        if (name == "output") { Reject([&] { model.Run(); }); Reject([&] { model.Output("y"); }); return 0; }
        model.Run(); auto output = model.Output("y"); Check(output.size == input.size);
        for (std::size_t i = 0; i < input.size; ++i) Check(output.data[i] == input.data[i]);
        if (name == "input") {
            input.data[0] = std::numeric_limits<float>::quiet_NaN();
            Reject([&] { model.Run(); }); Reject([&] { model.Output("y"); });
            input.data[0] = 5; model.Run(); Check(model.Output("y").data[0] == 5);
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
