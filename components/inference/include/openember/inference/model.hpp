#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openember::inference {
// This first CPU backend accepts float32 tensors only; shape may contain -1 in metadata.
struct TensorSpec { std::string name; std::vector<std::int64_t> shape; };
struct FloatView { float* data; std::size_t size; };
struct ConstFloatView { const float* data; std::size_t size; };
struct ModelOptions { int threads = 1; std::size_t max_tensor_elements = 1000000; };
class FloatModel {
 public:
    virtual ~FloatModel() = default;
    virtual const std::vector<TensorSpec>& Inputs() const = 0;
    virtual const std::vector<TensorSpec>& Outputs() const = 0;
    // One-shot, startup-only: exact names/ranks, positive concrete shapes. Resolves dynamic dimensions.
    virtual void Prepare(std::vector<TensorSpec> inputs, std::vector<TensorSpec> outputs) = 0;
    virtual FloatView Input(const std::string& name) = 0;
    virtual void Run() = 0;
    // Available only after a successful Run. A failed run invalidates previous outputs.
    virtual ConstFloatView Output(const std::string& name) const = 0;
};
// Single owner, synchronous. No model/policy/robot-specific types or SDK types in this API.
// Run cannot promise preemption or hard deadlines; callers must discard late results.
class OnnxModel final : public FloatModel {
 public:
    explicit OnnxModel(const std::string& path, ModelOptions options = {});
    ~OnnxModel() override;
    OnnxModel(const OnnxModel&) = delete;
    OnnxModel& operator=(const OnnxModel&) = delete;
    const std::vector<TensorSpec>& Inputs() const override;
    const std::vector<TensorSpec>& Outputs() const override;
    void Prepare(std::vector<TensorSpec> inputs, std::vector<TensorSpec> outputs) override;
    FloatView Input(const std::string& name) override;
    void Run() override;
    ConstFloatView Output(const std::string& name) const override;
    static std::string RuntimeVersion();
 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace openember::inference
