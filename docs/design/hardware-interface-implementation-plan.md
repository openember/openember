# OpenEmber Hardware Interface 实施与验收计划

状态：Draft  
版本：v0.2  
关联设计：

- [OpenEmber Hardware Interface 设计文档](./hardware-interface.md)
- [OpenEmber Sensor Framework 设计文档](./sensor-framework.md)

目标：把 Hardware Interface 从架构设计推进到可运行、可测试、可逐步扩展的实现，并为后续开发提供统一验收清单。

## 1. 总体验收目标

第一轮不是一次性接入所有真实硬件，而是建立一条稳定闭环：

```text
Mock hardware package
  -> components/sensor or components/actuator
  -> services/hardware_interface endpoint
  -> openember-msgs Protobuf
  -> OpenEmber Link
  -> listener / Product App
```

完成后应具备：

- 无硬件环境可构建、可运行、可测试。
- Product App 只依赖 Link 和 openember-msgs。
- `components/sensor`、`components/actuator` 不依赖 Link 和 Protobuf。
- Endpoint 能维护生命周期、状态、诊断和安全策略。
- 后续真实 hardware package 可以替换 mock package，而不改 Product App。

## 2. 实施原则

### 2.1 每个阶段都保持可构建

每个阶段完成后至少执行：

```bash
ember build
```

涉及 openember-msgs 时，还需要确认：

```text
Protobuf .pb.h / .pb.cc 被生成
openember::msgs_cpp 包含新增消息
OpenEmber target 正确链接 openember::msgs_cpp
```

### 2.2 先 Mock，后真实硬件

V1 优先实现：

```text
Mock IMU
Mock Temperature
Mock GNSS
Mock JointController
```

收益：

- 没有硬件也能跑通 Link、消息和 Endpoint 生命周期。
- CI 可以覆盖核心行为。
- 后续真实 package 只替换 component interface 实现。

### 2.3 Product App 只使用消息

Product App 不直接调用：

```text
IImu
ITemperatureSensor
IGnss
IJointController
LPIO
Hardware Package
```

Product App 只通过 Link：

```text
subscribe sensor state
publish actuator command
subscribe actuator state
query device state
observe diagnostics
```

### 2.4 Endpoint 不实现厂商协议

Endpoint 负责：

```text
config
lifecycle
Link publish / subscribe
message conversion
status / diagnostics
safety policy
```

Package 负责：

```text
device open
protocol parse
frame encode / decode
CRC / checksum
unit conversion
driver-specific error conversion
reconnect
```

### 2.5 JointController 是复合执行器

机器人产品中的 `JointController` 不拆成多个单关节 Endpoint。

推荐关系：

```text
JointControllerEndpoint
  -> IJointController
      -> JointController
          -> JointDevice[]
          -> JointDriver[]
          -> Bus[]
```

原因：

- 多关节同步发送需要统一控制边界。
- command timeout 和 safe output 必须整体协调。
- 多总线、多协议、多关节映射不能泄漏给 Product App。
- device_manager 可以把 `joint_controller0` 表达为 composite device，并把每个 joint 表达为 child device。

### 2.6 默认配置必须安全

执行器默认：

```text
disabled_on_start = true
safe_command = zero_torque
disable_on_stop = true
fault_latched = true
explicit_clear_fault = true
```

任何偏离默认安全行为的配置都必须显式写出。

## 3. 仓库边界

### 3.1 openember-msgs

负责语言无关消息定义：

```text
proto/openember/msgs/sensor/v1/sensor.proto
proto/openember/msgs/actuator/v1/actuator.proto
proto/openember/msgs/power/v1/power.proto
```

生成：

```text
.pb.h
.pb.cc
```

OpenEmber 通过 `openember::msgs_cpp` 链接这些生成代码。

### 3.2 openember

负责：

```text
components/hardware
components/sensor
components/actuator
services/hardware_interface
examples / tools
Kconfig / CMake
```

Mock package 可以先放在 OpenEmber 主仓库。

### 3.3 外部 Hardware Package

真实硬件建议后续独立 package：

```text
openember-pkg-imu-dm
openember-pkg-gnss-ublox
openember-pkg-joint-dm
openember-pkg-temperature-sensirion
```

这些 package 输出 C++ domain struct，不依赖 Protobuf、不创建 Link Node。

## 4. 目标目录

### 4.1 openember-msgs

```text
proto/openember/msgs/sensor/v1/sensor.proto
proto/openember/msgs/actuator/v1/actuator.proto
proto/openember/msgs/power/v1/power.proto
```

### 4.2 openember components

```text
components/
  hardware/
    include/openember/hardware/
      result.hpp
      error.hpp
      timestamp.hpp
      device_status.hpp
      lifecycle.hpp
    src/
    tests/

  sensor/
    include/openember/sensor/
      sensor.hpp
      sensor_info.hpp
      imu.hpp
      temperature.hpp
      gnss.hpp
      mock_imu.hpp
      mock_temperature.hpp
      mock_gnss.hpp
    src/
    tests/

  actuator/
    include/openember/actuator/
      actuator.hpp
      joint_controller.hpp
      joint_types.hpp
      mock_joint_controller.hpp
    src/
    tests/
```

`components/hardware` 是轻量公共层，不能依赖 Link、Protobuf 或 services。

### 4.3 openember service

```text
services/
  hardware_interface/
    CMakeLists.txt
    main.cpp
    include/openember/services/hardware_interface/
      endpoint.hpp
      endpoint_context.hpp
      endpoint_status.hpp
      endpoint_registry.hpp
      hardware_config.hpp
      hardware_interface_app.hpp
    src/
      endpoint.cpp
      endpoint_registry.cpp
      hardware_config.cpp
      hardware_interface_app.cpp
    endpoints/
      imu_endpoint.hpp
      imu_endpoint.cpp
      temperature_endpoint.hpp
      temperature_endpoint.cpp
      gnss_endpoint.hpp
      gnss_endpoint.cpp
      joint_controller_endpoint.hpp
      joint_controller_endpoint.cpp
      power_endpoint.hpp
      power_endpoint.cpp
    adapters/
      sensor_message_adapter.hpp
      sensor_message_adapter.cpp
      actuator_message_adapter.hpp
      actuator_message_adapter.cpp
      power_message_adapter.hpp
      power_message_adapter.cpp
    tests/
```

### 4.4 examples / tools

```text
examples/
  hardware_interface/
    mock_listener/
    joint_command_sender/

tools/
  hardware/
    hardware_info/
    joint_command/
    joint_state_listener/
```

V1 可以先做 examples，tools 后续补齐。

## 5. Phase 0：协议与配置准备

### 5.1 openember-msgs sensor/v1

新增消息：

```text
Vector3
ImuSample
TemperatureSample
GnssFix
SensorStatus
```

公共字段建议：

```text
Header header
string sensor_id
string frame_id
uint64 sample_time_monotonic_ns
uint64 sequence
```

标准单位：

```text
IMU acceleration: m/s^2
IMU angular_velocity: rad/s
temperature: deg C
GNSS latitude / longitude: degree
GNSS altitude: m
GNSS velocity: m/s
```

验收：

- `sensor.proto` 可被 protoc 编译。
- C++ 生成产物出现在 build generated 目录。
- `openember::msgs_cpp` 包含 sensor 生成代码。
- 示例代码可构造、`SerializeToString()`、`ParseFromArray()`。

### 5.2 openember-msgs actuator/v1

新增消息：

```text
JointCommand
JointCommandItem
JointState
JointStateItem
JointControllerStatus
ActuatorStatus
```

标准单位：

```text
position: rad
velocity: rad/s
effort: N*m
stiffness: N*m/rad
damping: N*m*s/rad
max_effort: N*m
```

验收：

- 支持按 `joint_id` 或 `joint_name` 表达关节。
- 支持 command sequence 和 command timestamp。
- 支持状态中的 `feedback_received`、`fault_code`、`temperature_celsius`。
- 可被 `JointControllerEndpoint` 订阅和发布。

### 5.3 openember-msgs power/v1

V1 可暂缓真实 endpoint，但消息规划应保留：

```text
BatteryState
PowerState
PowerCommand
```

验收：

- 如果暂缓实现，应在 proto TODO 和本计划中保持状态一致。

### 5.4 Kconfig / CMake skeleton

新增配置：

```text
OPENEMBER_COMPONENT_HARDWARE
OPENEMBER_COMPONENT_SENSOR
OPENEMBER_COMPONENT_ACTUATOR
OPENEMBER_SERVICE_HARDWARE_INTERFACE
OPENEMBER_HARDWARE_ENDPOINT_IMU
OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE
OPENEMBER_HARDWARE_ENDPOINT_GNSS
OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER
```

新增 target：

```text
openember_hardware
openember_sensor
openember_actuator
openember_hardware_interface
```

验收：

- `ember menuconfig` 中菜单位置正确。
- 禁用 service 后不编译 `openember_hardware_interface`。
- 禁用 endpoint 后不编译对应 endpoint 源码。
- `ember build` 通过。

## 6. Phase 1：components/hardware

目标：提供 sensor 和 actuator 共用的轻量基础类型。

建议接口：

```cpp
namespace openember::hardware {

enum class ErrorCode {
    kNone,
    kInvalidConfig,
    kOpenFailed,
    kTimeout,
    kDisconnected,
    kIoError,
    kProtocolError,
    kInvalidData,
    kNotSupported,
    kNotRunning,
    kSafetyViolation,
    kInternalError,
};

struct Error {
    ErrorCode code = ErrorCode::kNone;
    std::string message;
};

template <typename T>
class Result;

template <>
class Result<void>;

struct Timestamp {
    std::uint64_t monotonic_ns = 0;
};

}
```

验收：

- 不依赖 `core`。
- 不依赖 Link。
- 不依赖 Protobuf。
- 可被 `components/sensor` 和 `components/actuator` 链接。
- 有基础单元测试覆盖 success / error path。

## 7. Phase 2：components/sensor

### 7.1 基础接口

新增：

```text
ISensor
SensorInfo
SensorStatus
IImu
ITemperatureSensor
IGnss
```

关键约束：

- `Start()` / `Stop()` 幂等。
- `Read(timeout)` 不永久阻塞。
- `Stop()` 能唤醒阻塞中的 `Read()`。
- 数据使用 OpenEmber 标准单位。

验收：

- `components/sensor` target 可单独构建。
- 不依赖 `core` / Link / Protobuf。
- `MockImu` 可 Start / Read / Stop。
- `MockTemperature` 可 Start / Read / Stop。
- `MockGnss` 可 Start / Read / Stop。
- 单元测试覆盖 timeout、stop unblock、重复 start / stop。

### 7.2 Mock IMU

默认输出：

```text
acceleration = {0, 0, 9.80665} m/s^2
angular_velocity = {0, 0, 0} rad/s
temperature = 25 deg C
```

验收：

- 支持配置 `rate_hz`。
- sequence 单调递增。
- timestamp 单调递增。
- `Read(timeout)` 在启动后可稳定返回。

### 7.3 Mock Temperature

默认输出：

```text
temperature = 25 deg C
```

验收：

- 支持配置 `rate_hz`。
- 支持小幅模拟波动。
- sequence 单调递增。

### 7.4 Mock GNSS

默认输出：

```text
latitude_deg
longitude_deg
altitude_m
fix_type
satellites_used
```

验收：

- 支持配置固定坐标。
- 支持配置 `rate_hz`。
- sequence 单调递增。

## 8. Phase 3：Hardware Interface skeleton

新增：

```text
services/hardware_interface
```

可执行文件：

```text
openember_hardware_interface
```

基础行为：

- 初始化 OpenEmber Context。
- 创建 Node：`hardware_interface`。
- 读取配置。
- 创建 enabled endpoints。
- 启动 endpoints。
- 处理 SIGINT / SIGTERM。
- 停止 endpoints。

新增基础类型：

```text
HardwareEndpoint
EndpointContext
EndpointStatus
EndpointRegistry
HardwareConfig
HardwareInterfaceApp
```

验收：

- Kconfig 可启用 / 禁用 service。
- CMake 可构建可执行文件。
- 无 endpoint 配置时可以启动并优雅退出。
- 收到 SIGINT / SIGTERM 后所有 endpoint `Stop()`。
- Endpoint lifecycle 有单元测试。
- `Stop()` 幂等。
- `Start()` 失败后可 `Stop()` / 析构。
- Endpoint 状态可被 service 聚合。

## 9. Phase 4：Sensor Endpoints

### 9.1 Message adapters

Endpoint 内部使用 C++ struct，对外使用 Protobuf。

新增：

```text
SensorMessageAdapter
```

覆盖：

```text
ImuSample
TemperatureSample
GnssFix
SensorStatus
```

验收：

- 每个 adapter 有 round-trip 或字段映射测试。
- 标准单位正确。
- sequence、source node、sensor id、frame id、timestamp 正确填充。

### 9.2 ImuEndpoint

调用链：

```text
ImuEndpoint
  -> IImu
  -> Read(timeout)
  -> SensorMessageAdapter
  -> openember.msgs.sensor.v1.ImuSample
  -> Link publish
```

验收：

- 使用 MockImu 可发布 `/sensors/imu/imu0/sample`。
- listener 可 `ParseFromArray()`。
- 发布频率接近配置值。
- Stop 后采样线程退出。
- Read timeout 计入 diagnostics。

### 9.3 TemperatureEndpoint

验收：

- 使用 MockTemperature 可发布 `/sensors/temperature/temp0/sample`。
- 温度单位为 deg C。
- 支持低频发布。

### 9.4 GnssEndpoint

验收：

- 使用 MockGnss 可发布 `/sensors/gnss/gnss0/fix`。
- 包含 fix type、经纬度、高度和精度字段。
- 支持 1Hz 或更低频率。

## 10. Phase 5：诊断、设备视图和健康集成

### 10.1 device_manager 集成

建议映射：

```text
imu0
temp0
gnss0
joint_controller0
joint_controller0.FL_hip_joint
joint_controller0.FL_thigh_joint
```

验收：

- 每个 endpoint 有 `DeviceInfo`。
- 每个 endpoint 有 `DeviceState`。
- JointController child joints 有 `parent_id`。
- offline / degraded / error 状态能反映 endpoint 状态。

### 10.2 diagnostics / health_monitor 集成

Hardware Interface 发布：

```text
/diagnostics/hardware_interface
```

每个 Endpoint 维护：

```text
sample_count
publish_count
command_count
timeout_count
decode_error_count
io_error_count
last_error
last_sample_age_ms
last_command_age_ms
```

验收：

- DiagnosticArray 可被 listener 解析。
- Endpoint error 能体现在 diagnostics。
- stale sample / stale command 能被标记为 WARNING 或 STALE。
- health_monitor 可基于诊断标记 hardware degraded / error。

## 11. Phase 6：components/actuator

### 11.1 基础接口

新增：

```text
IActuator
IJointController
JointCommand
JointCommandItem
JointState
JointStateItem
JointControllerStatus
```

建议接口：

```cpp
namespace openember::actuator {

class IActuator {
public:
    virtual ~IActuator() = default;
    virtual hardware::Result<void> Start() = 0;
    virtual hardware::Result<void> Stop() = 0;
    virtual hardware::Result<void> Enable() = 0;
    virtual hardware::Result<void> Disable() = 0;
    virtual ActuatorStatus Status() const = 0;
};

class IJointController : public IActuator {
public:
    virtual hardware::Result<void> SetCommand(const JointCommand& command) = 0;
    virtual hardware::Result<void> Step() = 0;
    virtual hardware::Result<JointState> ReadState(std::chrono::milliseconds timeout) = 0;
    virtual hardware::Result<void> EmergencyStop() = 0;
    virtual hardware::Result<void> ClearFault() = 0;
};

}
```

验收：

- 不依赖 `core` / Link / Protobuf。
- 明确执行器安全语义。
- 有 `MockJointController`。
- `MockJointController` 支持 command / state 闭环。

### 11.2 Mock JointController

能力：

- 支持配置 joint 列表。
- 支持 `Enable()` / `Disable()`。
- 支持 `SetCommand()` / `Step()` / `ReadState()`。
- 支持 command timeout 测试。
- 支持 fault injection。

验收：

- 禁用状态下拒绝运动命令。
- command 超时后输出 safe state。
- state 中 joint 顺序稳定。
- sequence 单调递增。

## 12. Phase 7：JointControllerEndpoint

### 12.1 定位

`JointControllerEndpoint` 是复合执行器 Endpoint。

它不是单电机 Endpoint，也不是单关节 Endpoint。

调用链：

```text
JointControllerEndpoint
  -> subscribe JointCommand
  -> validate command
  -> cache latest command
  -> fixed-rate control loop
  -> IJointController::SetCommand()
  -> IJointController::Step()
  -> IJointController::ReadState()
  -> publish JointState
```

### 12.2 命令路径

要求：

- Subscriber 回调只解析、校验、缓存命令。
- 不在 subscriber 回调里直接 TX。
- 控制循环按固定频率执行 `Step()`。
- command 超时后进入 safe command。

验收：

- command topic 可控制 MockJointController。
- 非法 command 被拒绝并计数。
- command 超时后 state 显示 safe mode。
- endpoint stop 时先 disable，再释放 controller。

### 12.3 状态路径

要求：

- 固定频率发布 `JointState`。
- 支持反馈 timeout。
- 支持 per-joint fault code。
- 支持 child device state 上报。

验收：

- listener 可收到 JointState。
- joint 数量、joint id、joint name 与配置一致。
- fault injection 能进入 diagnostics。

### 12.4 安全策略

V1 必须实现：

```text
disabled_on_start
command_timeout
safe_command
disable_on_stop
reject_invalid_command
fault_latched
explicit_clear_fault
```

验收：

- 启动后默认不输出运动命令，除非配置明确 enable。
- Stop 时执行 Disable。
- EmergencyStop 后拒绝普通 command。
- ClearFault 必须通过显式 service 或 command。

## 13. Phase 8：Product App 集成

目标：让产品应用使用 Hardware Interface，而不是直接持有硬件对象。

`smart_device_demo` 可演化为：

```text
subscribe IMU / Temperature / GNSS
observe hardware diagnostics
publish high-level JointCommand when actuator endpoint enabled
maintain product state
```

验收：

- `smart_device_demo` 不包含具体硬件 package 头文件。
- `smart_device_demo` 可在 mock mode 下运行。
- `smart_device_demo` 可以在硬件服务未启动时给出清晰降级状态。
- Product App 的状态机或行为树只消费消息和系统状态。

## 14. Phase 9：真实硬件 package 接入

建议顺序：

1. 接入第一个真实 IMU package。
2. 接入第一个真实温度或 GNSS package。
3. 接入第一个真实 JointController package。
4. 增加硬件测试入口。

验收：

- Product App 不修改即可把 mock 切换为真实硬件。
- 真实硬件异常不会导致 runtime 崩溃。
- command timeout 和 safe output 在真实执行器上验证。
- device_manager 和 health_monitor 能观察真实硬件状态。

## 15. Phase 10：暂缓方向

以下内容不进入第一轮验收：

- 动态插件加载。
- 远程 package registry。
- Camera / LiDAR / Radar / Audio 高带宽流框架。
- 硬实时 executor。
- 复杂行为树或运动控制算法。
- 自动硬件发现。
- Web Console 控制面。
- 数据录制与回放。

这些能力可以在 Hardware Interface 基础稳定后逐步设计。

## 16. 测试计划

### 16.1 单元测试

覆盖：

- `hardware::Result`
- Sensor mock lifecycle
- Actuator mock lifecycle
- Endpoint lifecycle
- Message adapters
- Config parsing
- Joint command validation
- Command timeout
- Stop unblock

### 16.2 集成测试

最小测试拓扑：

```text
openember_hardware_interface
openember_msgs_listener
openember_joint_command_sender
smart_device_demo
```

验收：

- 能启动。
- 能发布 Mock IMU。
- 能发布 Mock Temperature。
- 能发布 Mock GNSS。
- 能接收 JointCommand。
- 能发布 JointState。
- Ctrl-C 后所有进程退出。

### 16.3 手工验收命令

建议保留：

```bash
ember menuconfig
ember update
ember build

./build/bin/openember_hardware_interface --config examples/hardware_interface/mock.yaml
./build/bin/openember_msgs_listener --topic /sensors/imu/imu0/sample
./build/bin/openember_joint_command_sender --topic /actuators/joints/joint_controller0/command
```

具体命令以后以实际 CLI 为准。

## 17. 阶段 Definition of Done

| 阶段 | 交付物 | DoD |
|------|--------|-----|
| Phase 0 | sensor / actuator proto，Kconfig / CMake skeleton | `ember build` 通过，示例可构造并序列化新消息 |
| Phase 1 | `components/hardware` | 不依赖 Link / Protobuf，基础测试通过 |
| Phase 2 | `components/sensor`，Mock IMU / Temperature / GNSS | Mock sensor lifecycle 测试通过 |
| Phase 3 | `openember_hardware_interface` skeleton | service 可启动、可停止、无 endpoint 可优雅退出 |
| Phase 4 | Imu / Temperature / GNSS Endpoints | mock sensor 数据可经 Link 发布并被 listener 解析 |
| Phase 5 | device_manager / health_monitor 集成 | DeviceState 和 DiagnosticArray 可观察 |
| Phase 6 | `components/actuator`，MockJointController | command / state 闭环测试通过 |
| Phase 7 | JointControllerEndpoint | command timeout、safe output、disable-on-stop 生效 |
| Phase 8 | Product App 集成 | Product App 只依赖消息，不依赖硬件 package |
| Phase 9 | 真实硬件 package | mock 可替换为真实硬件，异常可观测 |

## 18. 总体验收表

| 类别 | 验收项 | 必须阶段 |
|------|--------|----------|
| 构建 | `ember build` 通过 | 每阶段 |
| 消息 | sensor / actuator proto 生成 C++ | Phase 0 |
| 组件 | hardware 不依赖 Link / Protobuf | Phase 1 |
| 组件 | sensor 不依赖 Link / Protobuf | Phase 2 |
| 组件 | actuator 不依赖 Link / Protobuf | Phase 6 |
| 服务 | `openember_hardware_interface` 可启动 | Phase 3 |
| Endpoint | ImuEndpoint 发布 Mock IMU | Phase 4 |
| Endpoint | Temperature / GNSS 发布 Mock 数据 | Phase 4 |
| 诊断 | DiagnosticArray 可解析 | Phase 5 |
| 设备 | DeviceInfo / DeviceState 可查询 | Phase 5 |
| 执行器 | MockJointController 闭环 | Phase 6 |
| 执行器 | JointControllerEndpoint command/state 闭环 | Phase 7 |
| 安全 | command timeout 进入 safe output | Phase 7 |
| 安全 | Stop 调用 Disable | Phase 7 |
| 产品 | Product App 不依赖硬件 package | Phase 8 |
| 替换 | Mock 可替换真实 package | Phase 9 |

## 19. 关键风险与约束

### 19.1 Link subscriber 回调不能直接控制执行器

风险：

```text
subscriber callback 直接 TX
```

会导致控制周期不稳定，也不好实现 command timeout。

要求：

```text
callback 只解析和缓存 command；
control loop 统一下发。
```

### 19.2 JointController 必须整体管理

不要把每个 joint 拆成独立 Endpoint。

原因：

- 同步控制需要整体控制边界。
- 多总线路由需要统一配置。
- 安全策略需要整体执行。

### 19.3 Protobuf 不进入 package

Package 输出 C++ domain struct。

Protobuf 转换只在 Endpoint adapter 层。

### 19.4 Endpoint 不能变成产品行为容器

Endpoint 只做硬件接入和安全边界。

整机状态机、任务策略、行为树应放在 Product App。

### 19.5 高带宽设备暂缓

Camera、LiDAR、Radar、Audio 等高带宽设备暂缓。

它们未来可以复用 Endpoint / Package / Diagnostics 思想，但需要独立流式数据设计。

## 20. 建议下一步

下一步从 Phase 0 开始：

1. 在 openember-msgs 增加 `sensor/v1` 和 `actuator/v1` proto。
2. 在 OpenEmber 增加 Hardware Interface 的 Kconfig / CMake skeleton。
3. 实现 `components/hardware` 和 `components/sensor` 的 Mock IMU。
4. 实现 `openember_hardware_interface` + `ImuEndpoint`。

优先跑通：

```text
Mock IMU -> ImuEndpoint -> Protobuf -> Link -> listener
```

这条链路稳定后，再扩展 Temperature、GNSS、JointController、device_manager、health_monitor 和真实硬件 package。
