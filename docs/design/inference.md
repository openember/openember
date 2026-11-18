# 可选 CPU 推理组件

`openember::inference` 提供模型加载、float32 tensor 元数据、缓冲区准备与同步推理。
公共头文件 `openember/inference/model.hpp` 不暴露 ONNX Runtime 类型，也不包含机器人、
观测、关节或策略切换语义。首版后端为 `OnnxModel`，通过 `FloatModel` 可注入其他实现。

## 构建

`OPENEMBER_COMPONENT_INFERENCE` 默认关闭。通过 `OPENEMBER_ONNXRUNTIME_ROOT` 指定已安装 SDK，
不下载或安装推理依赖。如果使用生成的 `config.cmake`，需先在 menuconfig 中启用组件；
生成配置优先于 CMake cache。没有生成配置的纯 CMake 工程可用 `-DOPENEMBER_COMPONENT_INFERENCE=ON`。

```cmake
target_link_libraries(my_model_app PRIVATE openember::inference)
```

SDK 头文件可放在 `include` 或 `include/onnxruntime`，库放在 `lib` 或 `lib64`。
本机测试基线为 Linux x86_64、ONNX Runtime 1.20.1；其他 SDK/架构尚未验收。
交叉编译须使用目标 SDK，更换 SDK 时清理对应 include/library cache，保证头文件与运行库匹配。
SDK 文件和运行库不被自动复制进产品包。

## 生命周期与接口

1. 构造 `OnnxModel(path)`，读取实际输入/输出名称、形状与类型。
2. 启动阶段调用一次 `Prepare(inputs, outputs)`，验证精确名称、数量、rank 和静态维度，
   为动态维度给出正的具体值，并创建输入/输出缓冲区与 ORT tensor 包装。
3. 通过 `Input(name)` 返回的定长 view 填充数据，调用 `Run()`，成功后读取 `Output(name)`。
4. 错误通过异常返回；一次失败的 Run 会使之前输出失效，不能把旧输出当作本轮结果。

```cpp
namespace inf = openember::inference;
inf::OnnxModel model("model.onnx");
model.Prepare({{"observation", {1, 64}}}, {{"action", {1, 12}}});
auto input = model.Input("observation");
// 写入 input.data[0 .. input.size)。view 仅在模型生命周期内有效。
model.Run();
auto output = model.Output("action");
```

仅支持 float32 tensor，不支持 map/sequence、字符串、整数 I/O 或 scalar rank。
rank 上限 8，每 tensor 默认最多 1,000,000 个元素；名称、数量、维度均校验。
元数据的 `-1` 动态维度需在 Prepare 时绑定，例如 `[-1,55]` 绑定 `[1,55]`。
实际输出必须符合预分配形状，不能运行后静默更换布局。输入或输出出现 NaN/Inf 均拒绝。

对象由单一线程拥有。加载、图优化、Prepare 和预热放在控制周期之外。
默认 intra-op/inter-op 线程均为 1，使用 sequential 执行并关闭线程池 spinning，
不注册 GPU/NPU provider。可显式设置 intra-op 线程数 1–16，再测量抖动。
外层 tensor 存储与包装可复用，不意味着 ORT 内部或产品观测组装完全没有动态分配。
元素上限不限制模型权重或 ORT 内部工作内存。

同步 Run 不保证可中断或硬实时；调用者应在完成后再次检查样本年龄与截止时间，丢弃迟到结果。
不能用停止推理线程替代独立 HI/device watchdog。模型热切换、其他 provider、量化与 ncnn
后端均不属于本次实现。

## 验证

```bash
cmake -S components/inference/test -B /tmp/openember-inference-tests \
  -DOPENEMBER_ONNXRUNTIME_ROOT=/absolute/path/to/installed-sdk \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Werror'
cmake --build /tmp/openember-inference-tests -j4
ctest --test-dir /tmp/openember-inference-tests --output-on-failure
```

测试生成极小 ONNX protobuf，不需要 Python onnx/onnxruntime 包。
8 项测试覆盖静态/动态 tensor、形状/名字/容量错误、类型、非有限输入/输出、缺失模型，
以及配置导出。导出同时补齐遥控组件及 F710 工具的启停开关。

参考：[官方 C++ 文档](https://onnxruntime.ai/docs/get-started/with-cpp.html)、
[官方线程配置](https://onnxruntime.ai/docs/performance/tune-performance/threading.html)。
