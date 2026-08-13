# OpenEmber Hardware Interface 设计文档

状态：Draft  
版本：v0.2  
目标：为 OpenEmber 建立一套稳定、简洁、可扩展的硬件接入架构，使智能设备、机器人和边缘设备可以用统一的运行时边界接入传感器、执行器、电源和基础 I/O。

## 1. 背景

OpenEmber 面向的设备通常同时包含多类硬件：

- IMU、温度、GNSS、编码器、力传感器等观测设备。
- 电机、舵机、继电器、阀门等执行设备。
- 电池、PMU、风扇、灯光、电源 rail 等电源和辅助设备。
- GPIO、CAN、RS485、I2C、SPI、UART、PWM、OneWire 等平台外设。

只做一个 `Sensor Service` 无法自然覆盖执行器和电源控制；只做一个万能 `Device` 接口又会抹平不同硬件领域的关键语义，例如 IMU 的采样频率、GNSS 的 fix 状态、关节控制器的 command timeout 和 emergency stop。

因此 OpenEmber 推荐采用：

```text
Hardware Interface Service
  + Hardware Endpoint
  + Component Framework
  + Hardware Package
```

核心思想：

```text
Product App 只关心产品行为和消息；
Hardware Interface Service 承担运行时硬件边界；
Hardware Endpoint 承载具体硬件领域的接入逻辑；
Component Framework 定义类型安全的 C++ 领域接口；
Hardware Package 实现具体型号、厂商协议和硬件 I/O；
openember-msgs 定义跨语言、跨进程的消息契约。
```

## 2. Android HAL 经验与 OpenEmber 取舍

Android 的 HAL 不是单一形态。它同时是一组接口规范、实现机制、系统服务边界、兼容性要求和测试体系。不同硬件领域有不同调用路径：传感器通常经过 `SensorService -> Sensors HAL -> vendor implementation`，音频经过 `AudioFlinger / AudioPolicyService -> Audio HAL`，蓝牙经过系统蓝牙进程、协议栈和 Bluetooth HAL。

OpenEmber 可以借鉴 Android 的成功经验，但不需要复制它的 Binder、AIDL 或 VTS 体系。

值得借鉴：

- 用稳定接口隔离 framework 与具体硬件实现。
- 按硬件领域拆分接口，不设计一个万能硬件对象。
- 让产品应用面向高层消息和系统服务，不直接碰设备节点、寄存器和厂商帧。
- 为每个硬件领域保留状态、能力、诊断和测试入口。
- 把真实硬件实现放在可替换的 package 边界内。

OpenEmber 的对应关系：

| Android 经验 | OpenEmber 设计 |
|--------------|----------------|
| Framework API / system service | Product App 使用 OpenEmber Link 和 openember-msgs |
| HAL interface contract | `components/sensor`、`components/actuator`、`components/hardware` |
| Vendor HAL implementation | Hardware Package |
| Binderized HAL service | `services/hardware_interface` |
| Sensors HAL / Audio HAL 等领域划分 | `ImuEndpoint`、`GnssEndpoint`、`JointControllerEndpoint`、`PowerEndpoint` |
| CTS / VTS / diagnostics | OpenEmber 单元测试、集成测试、device_manager、health_monitor |

结论：

```text
OpenEmber Hardware Interface 不是“一个万能 HAL 类”。
它是一套接口契约、运行时服务、Endpoint 组织方式、消息协议和验收规则的组合。
```

## 3. 总体架构

OpenEmber 当前分层：

```text
Application Layer
  apps/
  system/
  services/
  examples/
  tools/

Middleware Layer
  communication/
  core/

Components Layer
  components/

Platform Layer
  platform/
```

Hardware Interface 的位置：

```text
Application Layer
  services/hardware_interface/
```

Component Framework 的位置：

```text
Components Layer
  components/hardware/
  components/sensor/
  components/actuator/
```

平台 I/O 的位置：

```text
Platform Layer
  platform/lpio/
```

推荐依赖方向：

```text
apps/<product_app>
  -> OpenEmber Link messages

services/hardware_interface
  -> core / communication / openember-msgs
  -> components/hardware
  -> components/sensor
  -> components/actuator
  -> hardware packages

hardware packages
  -> components/sensor or components/actuator
  -> platform/lpio

platform/lpio
  -> Linux kernel interfaces
```

禁止反向依赖：

```text
components/sensor 不依赖 Link / Protobuf / services
components/actuator 不依赖 Link / Protobuf / services
platform/lpio 不依赖 components / core / services
hardware package 不创建 OpenEmber Node
Product App 不依赖具体 hardware package
```

## 4. 核心概念

### 4.1 Hardware Interface Service

`Hardware Interface Service` 是长期运行的 OpenEmber service，建议可执行文件命名为：

```text
openember_hardware_interface
```

职责：

- 初始化 OpenEmber Context 和 Node。
- 读取硬件配置。
- 创建 Hardware Package 实例。
- 创建并管理多个 Hardware Endpoint。
- 通过 OpenEmber Link 发布传感器、执行器、电源和诊断消息。
- 通过 OpenEmber Link 接收执行器、电源和配置类命令。
- 向 `device_manager` 上报设备信息、状态和能力。
- 向 `health_monitor` 提供 endpoint 健康状态。
- 在退出时按安全顺序停止硬件。

它不负责：

- 实现产品行为。
- 实现姿态融合、导航、运动规划等算法。
- 直接替代 `device_manager`。
- 直接替代 `health_monitor`。
- 解析所有硬件领域的数据模型。

### 4.2 Hardware Endpoint

`Hardware Endpoint` 是一个硬件领域在 OpenEmber runtime 中的接入端点。

示例：

```text
ImuEndpoint
TemperatureEndpoint
GnssEndpoint
JointControllerEndpoint
PowerEndpoint
GpioEndpoint
```

Endpoint 负责：

- 将配置转换为 component/package 初始化参数。
- 管理硬件对象的运行时生命周期。
- 将 C++ domain struct 转换为 openember-msgs Protobuf message。
- 通过 Link 发布状态或样本。
- 通过 Link 接收命令。
- 维护 endpoint 状态、错误计数和诊断指标。
- 对执行器实施 command timeout、safe output、disable-on-stop 等安全策略。

Endpoint 不负责：

- 解析具体厂商协议。
- 直接操作寄存器或总线帧。
- 执行产品状态机。
- 做全局健康决策。

### 4.3 Component Framework

Component Framework 是 `components/` 下的纯 C++ 领域接口库。

建议分为：

```text
components/hardware
components/sensor
components/actuator
```

`components/hardware` 放公共基础类型：

```text
Result
Error
Timestamp
DeviceStatus
LifecycleState
```

`components/sensor` 放观测类接口：

```text
ISensor
IImu
ITemperatureSensor
IGnss
SensorInfo
ImuSample
TemperatureSample
GnssFix
```

`components/actuator` 放执行类接口：

```text
IActuator
IJointController
JointCommand
JointState
ActuatorStatus
```

Component Framework 必须保持轻量：

```text
不依赖 OpenEmber Link
不依赖 Protobuf
不创建 Node
不读取全局配置服务
不包含具体厂商协议
```

### 4.4 Hardware Package

`Hardware Package` 是具体硬件型号、厂商协议和设备 I/O 的实现边界。

示例：

```text
openember-pkg-imu-dm
openember-pkg-gnss-ublox
openember-pkg-temperature-sensirion
openember-pkg-joint-dm
```

Package 负责：

- 打开底层设备。
- 初始化硬件。
- 编码和解析协议帧。
- 校验 CRC 或 checksum。
- 完成单位转换。
- 管理 reconnect、buffer、线程和内部状态。
- 输出 OpenEmber C++ domain struct。

Package 不负责：

- 创建 OpenEmber Node。
- 发布或订阅 Link topic。
- 引入 openember-msgs Protobuf 类型。
- 管理产品行为。
- 替代系统节点。

Mock package 可以先放在 OpenEmber 主仓库，真实硬件 package 后续可以独立仓库维护。

### 4.5 Message Adapter

`Message Adapter` 位于 Hardware Interface Service 内部，负责：

```text
C++ domain struct <-> openember-msgs Protobuf message
```

这样可以保持：

```text
components 和 hardware package 不依赖 Protobuf；
Product App 不依赖 C++ hardware package；
Link 上的消息保持跨语言稳定。
```

## 5. 与系统节点的关系

### 5.1 launch_manager

`launch_manager` 负责启动和监督进程，不管理硬件对象。

推荐启动顺序：

```text
launch_manager
  -> config_service
  -> device_manager
  -> health_monitor
  -> hardware_interface
  -> product app
```

`launch_manager` 可以观察 `openember_hardware_interface` 是否 alive，并在进程异常退出时按策略重启或拉起故障流程。

### 5.2 config_service

`config_service` 负责配置和参数。

Hardware Interface 需要的配置包括：

```text
enabled endpoints
endpoint mode
driver id
device path
bus address
topic key
publish rate
command timeout
safety policy
diagnostics threshold
critical flag
```

V1 可以先支持本地 YAML 或静态配置；等 `config_service` 稳定后，再切换到统一参数服务。

### 5.3 device_manager

`device_manager` 提供运行时设备视图。

Hardware Interface 应上报：

```text
DeviceInfo
DeviceState
DeviceCapabilities
DeviceDiagnostics
```

`device_manager` 不负责：

- 打开硬件。
- 下发控制命令。
- 采样传感器。
- 解析厂商协议。
- 执行硬件安全策略。

复合执行器可以表达成父子设备：

```text
joint_controller0
joint_controller0.FL_hip_joint
joint_controller0.FL_thigh_joint
joint_controller0.FR_hip_joint
```

### 5.4 health_monitor

`health_monitor` 聚合系统健康状态。

Hardware Interface 应提供：

```text
endpoint alive
sample age
publish rate
command age
io error count
decode error count
timeout count
device disconnected
fault state
emergency state
```

`health_monitor` 根据这些信息判断系统是否进入：

```text
OK
DEGRADED
STALE
FAULT
EMERGENCY
```

### 5.5 product app

Product App 管理具体产品行为、整机状态机或行为树。

Product App 默认只通过 OpenEmber Link 和 openember-msgs 访问硬件：

```text
subscribe sensor state
subscribe actuator state
publish actuator command
query device state
observe diagnostics
```

Product App 不关心：

```text
/dev/ttyUSB0
/dev/i2c-1
CAN ID
baudrate
register map
vendor frame
checksum
```

## 6. 典型调用链

### 6.1 传感器数据路径

IMU 示例：

```text
IMU hardware
  -> hardware package
  -> components/sensor::IImu
  -> ImuEndpoint
  -> ImuMessageAdapter
  -> openember.msgs.sensor.v1.ImuSample
  -> OpenEmber Link topic
  -> Product App
```

温度和 GNSS 同理：

```text
Temperature hardware
  -> ITemperatureSensor
  -> TemperatureEndpoint
  -> TemperatureSample message
  -> Link

GNSS hardware
  -> IGnss
  -> GnssEndpoint
  -> GnssFix message
  -> Link
```

### 6.2 执行器命令路径

机器人关节控制器示例：

```text
Product App
  -> openember.msgs.actuator.v1.JointCommand
  -> OpenEmber Link topic
  -> JointControllerEndpoint subscriber
  -> validate and cache command
  -> fixed-rate control loop
  -> components/actuator::IJointController
  -> hardware package
  -> CAN / RS485 / EtherCAT / vendor bus
  -> motor drivers
```

状态返回路径：

```text
motor drivers
  -> hardware package
  -> IJointController::ReadState()
  -> JointControllerEndpoint
  -> JointMessageAdapter
  -> openember.msgs.actuator.v1.JointState
  -> Link
  -> Product App / health_monitor / logger
```

`JointControllerEndpoint` 是复合执行器 endpoint，不建议把每个 joint 拆成独立 endpoint。

原因：

- 多关节同步控制需要一个统一控制边界。
- 多 CAN、多协议、多关节映射不应泄漏给 Product App。
- command timeout、safe output、emergency stop 必须整体协调。
- device_manager 可以用父子设备表达细节，不需要拆 runtime endpoint。

### 6.3 电源路径

电源类设备通常既发布状态，也接收控制请求：

```text
PowerEndpoint
  -> publish BatteryState / PowerState
  -> subscribe PowerCommand
  -> control PMU / fan / light / power rail
```

电源安全策略应独立于普通传感器：

- 关键 power rail 的关闭必须有明确策略。
- 过温、欠压、过流应进入 diagnostics。
- Product App 可以请求动作，最终安全保护由 Endpoint/package 执行。

## 7. 目录结构

推荐 V1 目录：

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

examples/
  hardware_interface/
    mock_listener/
    joint_command_sender/

tools/
  hardware/
    hardware_info/
    joint_command/
```

V1 可以先实现 `components/hardware`、`components/sensor`、`services/hardware_interface`、IMU/Temperature/GNSS mock endpoint；执行器在后续阶段加入。

## 8. Kconfig 规划

Hardware Interface Service 属于：

```text
Application Layer
  Services
    Hardware Interface
```

领域接口属于：

```text
Components Layer
  Hardware
  Sensor Framework
  Actuator Framework
```

通信和消息仍属于：

```text
Middleware Layer
  Communication
    Link
    Messages
```

建议配置项：

```text
OPENEMBER_COMPONENT_HARDWARE
OPENEMBER_COMPONENT_SENSOR
OPENEMBER_COMPONENT_ACTUATOR

OPENEMBER_SERVICE_HARDWARE_INTERFACE
OPENEMBER_HARDWARE_ENDPOINT_IMU
OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE
OPENEMBER_HARDWARE_ENDPOINT_GNSS
OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER
OPENEMBER_HARDWARE_ENDPOINT_POWER
```

Kconfig 草案：

```text
config OPENEMBER_SERVICE_HARDWARE_INTERFACE
    bool "Hardware Interface"
    default y
    depends on OPENEMBER_ENABLE_LINK
    depends on OPENEMBER_ENABLE_MSGS

if OPENEMBER_SERVICE_HARDWARE_INTERFACE

menu "Sensor Endpoints"

config OPENEMBER_HARDWARE_ENDPOINT_IMU
    bool "IMU"
    default y
    depends on OPENEMBER_COMPONENT_SENSOR

config OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE
    bool "Temperature"
    default y
    depends on OPENEMBER_COMPONENT_SENSOR

config OPENEMBER_HARDWARE_ENDPOINT_GNSS
    bool "GNSS"
    default y
    depends on OPENEMBER_COMPONENT_SENSOR

endmenu

menu "Actuator Endpoints"

config OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER
    bool "Joint Controller"
    default n
    depends on OPENEMBER_COMPONENT_ACTUATOR

endmenu

endif
```

默认建议：

- `Hardware Interface` 可以默认开启，便于形成标准系统拓扑。
- Mock sensor endpoints 可以默认开启，方便无硬件开发。
- `JointControllerEndpoint` 默认关闭，避免普通智能设备默认引入执行器安全语义。
- 真实硬件 package 由产品工程或 Third party / bundle 配置选择。

## 9. Endpoint 生命周期

Endpoint 生命周期必须一致：

```text
create
  -> configure
  -> start
  -> running
  -> stop
  -> stopped
```

建议基础接口：

```cpp
namespace openember::services::hardware_interface {

enum class EndpointState {
    kCreated,
    kConfigured,
    kRunning,
    kStopping,
    kStopped,
    kError,
};

struct EndpointStatus {
    EndpointState state = EndpointState::kCreated;
    std::string endpoint_id;
    std::string device_id;
    std::string driver;
    std::string mode;
    std::string message;
    std::uint64_t sequence = 0;
    std::uint64_t sample_count = 0;
    std::uint64_t publish_count = 0;
    std::uint64_t command_count = 0;
    std::uint64_t error_count = 0;
    std::uint64_t last_sample_monotonic_ns = 0;
    std::uint64_t last_publish_monotonic_ns = 0;
    std::uint64_t last_command_monotonic_ns = 0;
};

class HardwareEndpoint {
public:
    virtual ~HardwareEndpoint() = default;

    virtual bool Configure() = 0;
    virtual bool Start() = 0;
    virtual void Stop() noexcept = 0;
    virtual EndpointStatus Status() const = 0;
};

}
```

约束：

- 构造函数不打开硬件、不启动线程。
- `Configure()` 只做配置校验和轻量准备。
- `Start()` 可以打开硬件和启动线程。
- `Stop()` 必须幂等，并尽快返回。
- 析构必须释放资源，且不得抛异常。
- `Start()` 失败后对象仍可安全 `Stop()` 和析构。
- 执行器 endpoint 停止时必须先进入安全输出，再释放硬件对象。

## 10. EndpointContext

Endpoint 需要访问 Node、配置、日志、设备状态上报等能力，但不应使用全局单例。

建议由 Hardware Interface Service 创建：

```cpp
namespace openember::services::hardware_interface {

struct EndpointContext {
    openember::Node& node;
    HardwareConfig config;
    DeviceReporter* device_reporter = nullptr;
    DiagnosticReporter* diagnostic_reporter = nullptr;
};

}
```

原则：

- Endpoint 可以使用 Link。
- Endpoint 可以发布诊断。
- Endpoint 不直接持有全局 runtime。
- Endpoint 不直接访问其他 Endpoint 的内部对象。
- V1 如果 reporter 尚未稳定，可以先置空或用轻量 mock。

## 11. 消息与 Topic

Hardware Interface 使用 openember-msgs 作为跨语言消息定义。

推荐消息域：

```text
openember.msgs.sensor.v1.ImuSample
openember.msgs.sensor.v1.TemperatureSample
openember.msgs.sensor.v1.GnssFix

openember.msgs.actuator.v1.JointCommand
openember.msgs.actuator.v1.JointState
openember.msgs.actuator.v1.JointControllerStatus

openember.msgs.power.v1.BatteryState
openember.msgs.power.v1.PowerState
openember.msgs.power.v1.PowerCommand
```

推荐 topic key：

```text
/sensors/imu/<name>/sample
/sensors/temperature/<name>/sample
/sensors/gnss/<name>/fix

/actuators/joints/<name>/command
/actuators/joints/<name>/state
/actuators/joints/<name>/status

/power/<name>/command
/power/<name>/state

/diagnostics/hardware_interface
```

约束：

- Endpoint 内部使用 C++ domain struct。
- Link 发布使用 Protobuf message。
- Protobuf 只在 service adapter 层出现。
- Package 不依赖 openember-msgs。
- Topic key 以后应由 OpenEmber Link 的命名工具统一添加 namespace、device id 或部署前缀。

## 12. 配置模型

建议按 endpoint 划分配置：

```yaml
hardware_interface:
  enabled: true

  endpoints:
    imu0:
      type: imu
      enabled: true
      mode: mock
      driver: mock_imu
      frame_id: imu_link
      topic: /sensors/imu/imu0/sample
      publish_rate_hz: 100
      critical: false
      config:
        rate_hz: 100

    temp0:
      type: temperature
      enabled: true
      mode: mock
      driver: mock_temperature
      topic: /sensors/temperature/temp0/sample
      publish_rate_hz: 2
      critical: false
      config:
        initial_temperature_celsius: 25.0

    gnss0:
      type: gnss
      enabled: true
      mode: mock
      driver: mock_gnss
      topic: /sensors/gnss/gnss0/fix
      publish_rate_hz: 1
      critical: false
      config:
        latitude_deg: 31.2304
        longitude_deg: 121.4737
        altitude_m: 10.0

    joint_controller0:
      type: joint_controller
      enabled: false
      mode: mock
      driver: mock_joint_controller
      command_topic: /actuators/joints/joint_controller0/command
      state_topic: /actuators/joints/joint_controller0/state
      publish_rate_hz: 100
      command_timeout_ms: 100
      safe_mode: zero_torque
      disabled_on_start: true
      disable_on_stop: true
      critical: true
      config:
        joints:
          - id: 1
            name: FL_hip_joint
          - id: 2
            name: FL_thigh_joint
```

公共字段：

```text
type
enabled
mode
driver
topic
command_topic
state_topic
publish_rate_hz
command_timeout_ms
safe_mode
frame_id
critical
config
```

`config:` 内部字段由具体 package 解释。

## 13. Runtime 模式

长期应支持 endpoint 级别 mode：

| 模式 | 语义 |
|------|------|
| `mock` | 本进程 mock package，用于无硬件开发和 CI |
| `simulation` | 接入仿真数据或仿真执行器 |
| `robot` | 打开真实硬件 |

不要把 mode 做成全局唯一限制。实际系统可能混合：

```text
真实关节控制器 + Mock GNSS
真实 IMU + 仿真温度
```

V1 先实现 `mock`。

## 14. 可靠性与安全

Hardware Interface 是硬件运行时边界，可靠性要求高于普通 example。

通用要求：

- Endpoint 启动失败必须有明确日志和状态。
- 单个非关键 Endpoint 失败不应默认导致整个进程崩溃。
- 关键 Endpoint 是否导致进程退出由配置决定。
- 所有后台线程必须可 join。
- 所有阻塞读必须可被 `Stop()` 打断。
- 设备断开不应造成未捕获异常穿透线程边界。
- 错误计数和最近错误必须可观测。

传感器要求：

- 采样超时可计数并上报。
- 高频数据默认丢旧保新。
- 时间戳必须单调递增，或明确标记异常。
- 单位必须转换为 OpenEmber 标准单位。

执行器要求：

- 默认不上电或不 enable。
- command timeout 后进入 safe output。
- disable 必须优先于释放资源。
- emergency stop 必须高优先级处理。
- fault reset 必须显式。
- 非法命令不得下发到底层硬件。
- Subscriber 回调只解析、校验、缓存命令，不直接进行硬件 TX。

## 15. 诊断与状态

每个 Endpoint 应维护：

```text
endpoint_id
device_id
state
driver
mode
sample_count
publish_count
command_count
error_count
last_error
last_sample_age_ms
last_command_age_ms
io_error_count
decode_error_count
timeout_count
fault_code
```

Hardware Interface Service 聚合：

```text
endpoint_count
running_endpoint_count
failed_endpoint_count
critical_failed_endpoint_count
```

这些信息应进入：

```text
device_manager
health_monitor
/diagnostics/hardware_interface
```

## 16. 高带宽设备边界

Camera、LiDAR、Radar、Audio 等高带宽或强实时流设备不进入 V1 Hardware Interface 的核心目标。

原因：

- 数据量和传输机制不同。
- 可能需要共享内存、零拷贝、压缩或专用流框架。
- 可能需要独立的时钟同步和 buffer 策略。

长期可以借鉴同一套思想：

```text
稳定组件接口
运行时 endpoint
可替换 package
状态与诊断
```

但不应把它们硬塞进 sensor V1 或 actuator V1。

## 17. 非目标

V1 不做：

- 不实现复杂 actor runtime。
- 不引入全局共享数据板作为公共 API。
- 不做动态插件加载。
- 不做远程 package registry。
- 不做 Camera、LiDAR、Radar、Audio 高带宽流框架。
- 不做 SLAM、导航、路径规划、姿态融合。
- 不把 product behavior 放进 Hardware Interface。
- 不让 Product App 直接依赖具体硬件 package。
- 不为所有硬件设计一个万能 `HardwareDevice` 数据模型。

## 18. 命名规范

正式术语：

```text
Hardware Interface Service
Hardware Endpoint
Sensor Endpoint
Actuator Endpoint
Power Endpoint
Hardware Package
Message Adapter
```

推荐类名：

```text
HardwareInterfaceApp
HardwareEndpoint
ImuEndpoint
TemperatureEndpoint
GnssEndpoint
JointControllerEndpoint
PowerEndpoint
SensorMessageAdapter
ActuatorMessageAdapter
PowerMessageAdapter
```

不推荐：

```text
Bridge
DriverEndpoint
SensorService
HardwareManager
UniversalDevice
```

原因：

- `Bridge` 容易让人联想到临时胶水层或 legacy bridge。
- `Driver` 应留给具体硬件 package。
- `SensorService` 语义太窄。
- `Manager` 太泛，容易变成大对象。
- `UniversalDevice` 容易引导出万能接口。

## 19. 结论

OpenEmber Hardware Interface 的长期方向是：

```text
面向 Product App 暴露稳定消息；
面向硬件实现暴露稳定 C++ component interface；
用 services/hardware_interface 管理运行时 endpoint；
用 hardware package 隔离具体协议和设备差异；
用 device_manager / health_monitor 提供系统可观察性。
```

这套设计可以覆盖两类常见产品：

```text
简单智能设备
  -> 启用少量 sensor / power endpoints
  -> Product App 订阅状态并执行业务逻辑

复杂机器人
  -> 启用 IMU / GNSS / JointController / Power endpoints
  -> Product App 管理整机状态机或行为树
  -> Hardware Interface 保证硬件接入、安全策略和诊断边界
```

第一轮实施应优先跑通：

```text
Mock IMU -> ImuEndpoint -> Protobuf -> OpenEmber Link -> listener
```

这条链路跑通后，再扩展 Temperature、GNSS、JointController 和真实硬件 package。
