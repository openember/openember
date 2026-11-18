#include "openember/inference/model.hpp"
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace openember::inference {
namespace {
std::size_t Elements(const std::vector<std::int64_t>& shape, std::size_t limit) {
    if (shape.empty() || shape.size() > 8) throw std::invalid_argument("unsupported tensor rank");
    std::size_t count = 1;
    for (auto dimension : shape) {
        if (dimension <= 0 || static_cast<std::uint64_t>(dimension) > limit / count)
            throw std::invalid_argument("tensor shape is dynamic, empty or exceeds allocation bound");
        count *= static_cast<std::size_t>(dimension);
    }
    return count;
}
struct Buffer { TensorSpec spec; std::vector<float> values; };
}
struct OnnxModel::Impl {
    ModelOptions options;
    Ort::Env environment{ORT_LOGGING_LEVEL_WARNING, "openember-inference"};
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<TensorSpec> inputs, outputs;
    std::vector<Buffer> input_buffers, output_buffers;
    std::vector<Ort::Value> input_tensors, output_tensors;
    std::vector<const char*> input_names, output_names;
    bool attempted = false, prepared = false, valid_output = false;
    Impl(const std::string& path, ModelOptions settings) : options(settings) {
        if (path.empty() || options.threads < 1 || options.threads > 16 || !options.max_tensor_elements ||
            options.max_tensor_elements > 100000000)
            throw std::invalid_argument("invalid inference options");
        Ort::SessionOptions config;
        config.SetIntraOpNumThreads(options.threads); config.SetInterOpNumThreads(1);
        config.SetExecutionMode(ORT_SEQUENTIAL);
        config.AddConfigEntry("session.intra_op.allow_spinning", "0");
        config.AddConfigEntry("session.inter_op.allow_spinning", "0");
        session = std::make_unique<Ort::Session>(environment, path.c_str(), config);
        Ort::AllocatorWithDefaultOptions allocator;
        for (bool input : {true, false}) {
            const auto count = input ? session->GetInputCount() : session->GetOutputCount();
            if (!count || count > 64) throw std::invalid_argument("unsupported model I/O count");
            std::set<std::string> names;
            auto& specs = input ? inputs : outputs;
            for (std::size_t i = 0; i < count; ++i) {
                auto name = input ? session->GetInputNameAllocated(i, allocator) : session->GetOutputNameAllocated(i, allocator);
                auto type = input ? session->GetInputTypeInfo(i) : session->GetOutputTypeInfo(i);
                if (type.GetONNXType() != ONNX_TYPE_TENSOR) throw std::invalid_argument("model I/O must be tensors");
                auto tensor = type.GetTensorTypeAndShapeInfo();
                if (tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                    throw std::invalid_argument("model I/O must be float32");
                if (!name || !*name.get() || !names.insert(name.get()).second)
                    throw std::invalid_argument("invalid model tensor name");
                specs.push_back({name.get(), tensor.GetShape()});
            }
        }
    }
    std::vector<Buffer> Allocate(const std::vector<TensorSpec>& actual, const std::vector<TensorSpec>& metadata) {
        if (actual.size() != metadata.size()) throw std::invalid_argument("model tensor count mismatch");
        std::vector<Buffer> buffers;
        std::set<std::string> seen;
        for (const auto& spec : actual) {
            const auto found = std::find_if(metadata.begin(), metadata.end(), [&](const auto& m) { return m.name == spec.name; });
            if (found == metadata.end() || !seen.insert(spec.name).second || found->shape.size() != spec.shape.size())
                throw std::invalid_argument("model tensor name/rank mismatch");
            const auto count = Elements(spec.shape, options.max_tensor_elements);
            for (std::size_t i = 0; i < spec.shape.size(); ++i)
                if (found->shape[i] != -1 && found->shape[i] != spec.shape[i])
                    throw std::invalid_argument("model tensor shape mismatch: " + spec.name);
            buffers.push_back({spec, std::vector<float>(count, 0)});
        }
        return buffers;
    }
    void Bind(std::vector<Buffer>& buffers, std::vector<Ort::Value>& tensors, std::vector<const char*>& names) {
        tensors.reserve(buffers.size()); names.reserve(buffers.size());
        for (auto& buffer : buffers) {
            names.push_back(buffer.spec.name.c_str());
            tensors.push_back(Ort::Value::CreateTensor<float>(memory, buffer.values.data(), buffer.values.size(),
                buffer.spec.shape.data(), buffer.spec.shape.size()));
        }
    }
};
OnnxModel::OnnxModel(const std::string& path, ModelOptions options) : impl_(std::make_unique<Impl>(path, options)) {}
OnnxModel::~OnnxModel() = default;
const std::vector<TensorSpec>& OnnxModel::Inputs() const { return impl_->inputs; }
const std::vector<TensorSpec>& OnnxModel::Outputs() const { return impl_->outputs; }
std::string OnnxModel::RuntimeVersion() { return OrtGetApiBase()->GetVersionString(); }
void OnnxModel::Prepare(std::vector<TensorSpec> inputs, std::vector<TensorSpec> outputs) {
    auto& p = *impl_;
    if (p.attempted) throw std::logic_error("model preparation already attempted");
    auto in = p.Allocate(inputs, p.inputs), out = p.Allocate(outputs, p.outputs);
    // Invalidate this object if allocation/binding throws; never reuse partially bound buffers.
    p.attempted = true;
    p.input_buffers = std::move(in); p.output_buffers = std::move(out);
    p.Bind(p.input_buffers, p.input_tensors, p.input_names); p.Bind(p.output_buffers, p.output_tensors, p.output_names);
    p.prepared = true;
}
FloatView OnnxModel::Input(const std::string& name) {
    for (auto& buffer : impl_->input_buffers)
        if (buffer.spec.name == name) return {buffer.values.data(), buffer.values.size()};
    throw std::invalid_argument("unknown or unprepared model input: " + name);
}
void OnnxModel::Run() {
    auto& p = *impl_; p.valid_output = false;
    if (!p.prepared || p.input_tensors.size() != p.inputs.size() || p.output_tensors.size() != p.outputs.size())
        throw std::logic_error("model buffers not prepared");
    for (const auto& input : p.input_buffers)
        for (auto value : input.values)
            if (!std::isfinite(value)) throw std::invalid_argument("nonfinite model input");
    p.session->Run(Ort::RunOptions{nullptr}, p.input_names.data(), p.input_tensors.data(), p.input_tensors.size(),
        p.output_names.data(), p.output_tensors.data(), p.output_tensors.size());
    for (const auto& output : p.output_buffers)
        for (auto value : output.values)
            if (!std::isfinite(value)) throw std::runtime_error("nonfinite model output");
    p.valid_output = true;
}
ConstFloatView OnnxModel::Output(const std::string& name) const {
    if (!impl_->valid_output) throw std::logic_error("no successful model result");
    for (const auto& buffer : impl_->output_buffers)
        if (buffer.spec.name == name) return {buffer.values.data(), buffer.values.size()};
    throw std::invalid_argument("unknown model output: " + name);
}
}  // namespace openember::inference
