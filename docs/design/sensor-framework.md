# OpenEmber Sensor Framework 设计文档

状态：Draft  
版本：v0.3  
目标：基于当前 OpenEmber 分层架构，建立一个轻量、类型安全、可测试、可扩展的传感器接入框架，并首先完成 IMU 最小闭环。

## 1. 背景

OpenEmber 面向智能设备、机器人、边缘网关和工业控制等场景。实际产品中通常会接入大量传感器：

- IMU
- GNSS
- 温度、湿度、压力传感器
- 编码器
- 力/力矩传感器
- 流量计
- 重量传感器

同一种传感器又会有大量不同厂商和型号。以 IMU 为例，不同设备可能通过 UART、SPI、I2C、CAN 等接口通信，并拥有完全不同的数据帧、初始化命令、校验方式和配置参数。

Sensor Framework 的目标不是统一这些底层协议，而是在具体协议之上提供稳定的 OpenEmber C++ 接口，使上层代码能够尽量脱离具体硬件型号。

核心原则：

```text
Sensor Framework 定义“传感器是什么”；
Sensor Package 解决“具体设备怎么用”；
Sensor Service 负责“什么时候采样、发布到哪里”；
device_manager 负责“运行时设备如何被系统观察和查询”。
```

## 2. 当前 OpenEmber 分层定位

OpenEmber 当前主干分层为：

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

Sensor Framework 应放在：

```text
components/sensor/
```

原因：

- 它是可复用 C++ 库。
- 它不属于 OpenEmber runtime core。
- 它不直接创建 Link node。
- 它不直接发布 topic 或提供 service。
- 它理解传感器领域语义，因此不属于 `platform/lpio`。
- 它不负责系统运行时设备注册，因此不属于 `system/device_manager`。

### 2.1 与 Platform Layer 的关系

`platform/lpio` 提供 Linux 外设 I/O：

```text
SerialPort
RS485
SBUSReader
I2CBus
SPIBus
CANBus
GPIO
PWM
OneWire
```

LPIO 只理解字节、总线、fd、GPIO line、sysfs 等平台资源，不理解 IMU、GNSS 或温度传感器协议。

Sensor Package 可以依赖 LPIO，例如：

```text
DM IMU package
  -> lpio::SerialPort

BMI088 package
  -> lpio::SPIBus
```

Sensor Framework 本身应尽量不直接依赖某个具体 LPIO 设备类。V1 可让具体 package 自己持有或创建 LPIO 对象。

### 2.2 与 Middleware Layer 的关系

`core/` 提供 Node、Topic、Service、Context、Link 接入等 middleware 能力。

Sensor Framework 不依赖 `core`：

```text
components/sensor 不能依赖 openember::Node
components/sensor 不能直接 publish
components/sensor 不能直接 subscribe
components/sensor 不能直接使用 Protobuf runtime 对象
```

需要发布传感器数据时，应由 Application Layer 中的 service 或 app 完成，例如：

```text
services/sensor/imu_publisher
  -> 使用 components/sensor 的 IImu
  -> 转换为 openember-msgs 的 sensor/v1 消息
  -> 通过 OpenEmber Link 发布
```

### 2.3 与 system/device_manager 的关系

当前 `system/device_manager` 已经是运行时系统节点，负责发布和查询：

```text
DeviceInfo
DeviceState
DeviceQuery
```

Sensor Framework 不替代 `device_manager`。

推荐边界：

```text
components/sensor
  C++ 驱动接口、样本类型、生命周期、registry、manager

Sensor Package
  具体硬件协议、初始化、单位换算、错误转换

system/device_manager
  运行时设备注册、DeviceInfo/DeviceState/DeviceQuery

services/sensor/imu_publisher
  创建传感器对象、采样、转换消息、发布 topic、向 device_manager 暴露设备状态
```

第一版可以先让 `device_manager` 维持现有的静态/配置式设备视图。等 Sensor Framework 和 IMU package 稳定后，再补一个 adapter，把传感器实例状态转换为 `DeviceInfo/DeviceState`。

## 3. 设计目标

V1 目标：

1. 在 `components/sensor` 中提供最小稳定 C++ API。
2. 首先支持 IMU 类型接口。
3. 提供 `ISensor`、`IImu`、`SensorManager`、`ImuRegistry`。
4. 明确传感器生命周期、错误模型、时间戳和标准单位。
5. 支持 Mock IMU，用于无硬件开发和 CI。
6. 支持独立 Sensor Package 接入具体设备型号。
7. 产品只编译实际需要的 Sensor Package。
8. 允许上层替换不同型号 IMU 时不修改采样服务和业务代码。

V1 首先完成：

```text
Mock IMU
  -> IImu
  -> imu_reader example
  -> imu_publisher service
  -> OpenEmber Link topic
```

真实硬件 package 可随后接入，例如 DM-IMU-L1。

## 4. 非目标

V1 不做以下事情：

- 不设计万能 `SensorData read()`。
- 不设计 `std::variant<ImuData, GnssData, ImageData, ...>`。
- 不解析任何厂商协议。
- 不实现 package manager。
- 不运行时扫描插件目录。
- 不使用 `dlopen()` 动态加载驱动。
- 不实现姿态解算、EKF、SLAM、融合算法。
- 不处理 Camera、LiDAR、Radar 等高带宽流式设备。
- 不直接依赖 OpenEmber Link。
- 不直接发布 middleware topic。
- 不替代 `device_manager`。

Camera、LiDAR、Radar 未来应进入独立的 Stream Framework 或 Perception Framework。它们可以复用 Device、Package、生命周期等思想，但不应挤进 V1 Sensor Framework。

## 4.1 RT-Thread Sensor Framework 借鉴与取舍

RT-Thread 的 sensor framework 是一个面向 RTOS 的 C 设备框架。它把传感器注册为 `rt_device`，并通过 `open/read/control`、设备 flags、IRQ、FIFO、命令行工具等机制与 RT-Thread 内核设备模型集成。

OpenEmber 不应照搬它的形态，因为 OpenEmber 当前定位是 Linux 上的 C++ 应用框架和 middleware framework。OpenEmber 更适合保持 RAII、类型安全、组件解耦和显式生命周期管理。

但是 RT-Thread 的一些设计思想很有参考价值。

### 4.1.1 传感器元数据

RT-Thread v2 使用统一的 sensor info 描述传感器类型、厂商、单位、接口类型、模式、FIFO 深度、最小采样周期、精度和量程。

OpenEmber 应吸收这个思想，把 `SensorInfo` 设计成可被 `device_manager` 和诊断工具复用的能力描述，而不仅是一个名字字符串。

建议长期包含：

```text
sensor_id
sensor_type
vendor
model
driver
frame_id
bus_type
unit
range
resolution
min_period
fifo_depth
supported_fetch_modes
capabilities
```

这些字段不要求 V1 一次性全部实现完整，但 public API 命名和结构应为后续扩展预留空间。

### 4.1.2 数据获取模式

RT-Thread 区分：

```text
polling
interrupt
fifo
```

OpenEmber 也应在概念上保留这三类能力，但不要把 RTOS 的 IRQ pin、device flag 或 bit-packed mode 直接暴露到框架层。

建议 OpenEmber 使用更贴近 Linux 和 C++ 的表达：

```cpp
enum class SensorFetchMode {
    kPolling,
    kEvent,
    kFifo,
};
```

V1 public API 仍保持简单的阻塞式 Pull：

```cpp
auto sample = imu.Read(timeout);
```

具体 package 可以在内部选择串口阻塞读、`poll/epoll`、IIO buffer、GPIO event、CAN receive queue 或后台线程。Sensor Framework 只表达能力和语义，不规定底层触发方式。

### 4.1.3 复合传感器模块

RT-Thread 的 `sensor_module` 可以把多个 sensor 绑定到同一个物理模块中，共享锁、buffer 和中断回调。这对 IMU、环境传感器和组合芯片很有价值。

OpenEmber 应保留类似思想，但建议用 C++ package 内部组合实现，而不是做成全局设备框架：

```text
SensorGroup / SensorModule
  shared bus
  shared fd
  shared mutex
  shared IRQ/event source
  shared parser or register map
```

例如：

```text
ICM42688 package
  -> IImu

BME280 package
  -> ITemperatureSensor
  -> IPressureSensor
  -> IHumiditySensor
```

V1 可以暂缓公开 `SensorGroup` API，但真实硬件 package 的内部设计应避免为同一颗芯片重复打开总线、重复创建线程或重复抢占中断资源。

### 4.1.4 最小驱动操作面

RT-Thread 驱动核心接口接近：

```text
fetch_data()
control()
```

这个思想值得保留：底层 package 的必需接口应该小而清晰。

OpenEmber 不建议使用裸 `control(cmd, void*)`，而应使用类型安全接口：

```cpp
Result<void> Start();
Result<void> Stop();
Result<ImuSample> Read(std::chrono::milliseconds timeout);
Result<void> Configure(const ImuConfig& config);
const SensorInfo& Info() const;
SensorStatus Status() const;
```

其中 `Configure()` 不要求进入 V1 `ISensor` 基类。后续如果需要设置量程、采样率、滤波带宽，应优先使用明确的配置结构或枚举，而不是整数命令码。

### 4.1.5 设备发现与诊断工具

RT-Thread 通过统一设备注册和 `sensor_cmd` 提供传感器查询、信息打印和数据读取能力。

OpenEmber 的对应关系应是：

```text
components/sensor
  提供 SensorInfo / SensorStatus

system/device_manager
  聚合运行时 DeviceInfo / DeviceState / DeviceQuery

tools 或 examples/sensor
  提供 sensor list / sensor info / sensor read 工具
```

推荐后续提供：

```text
openember_sensor_list
openember_sensor_info
openember_imu_reader
```

这些工具对机器人和智能设备现场调试非常重要，尤其是在硬件连接、采样率、单位换算、坐标系和时间戳排查阶段。

### 4.1.6 不照搬的内容

OpenEmber 不应照搬以下设计：

- 不使用大一统 `SensorData` union 承载所有传感器数据。
- 不把所有传感器读数塞进 `std::variant<...>` 作为 V1 public API。
- 不照搬 `open/read/control` 的 C 设备模型。
- 不暴露裸 `cmd + void*` 控制接口。
- 不使用 bit-packed mode 表达采样、精度和电源模式。
- 不把 GPIO IRQ 细节放进通用 Sensor Framework。
- 不默认使用硬件原始单位，例如 `mg`、`mdps`。
- 不让 `components/sensor` 直接依赖 Link、Protobuf 或 runtime node。

OpenEmber 的取舍是：

```text
借鉴 RT-Thread 的元数据、采样模式、复合模块和诊断工具思想；
保留 OpenEmber 自己的 C++ RAII、类型安全、分层解耦和消息发布边界。
```

## 5. 推荐目录结构

OpenEmber 主仓库内：

```text
components/
  sensor/
    CMakeLists.txt
    include/openember/sensor/
      core/
        sensor.hpp
        sensor_error.hpp
        sensor_info.hpp
        sensor_status.hpp
        sensor_timestamp.hpp
        sensor_manager.hpp
        result.hpp
      imu/
        imu.hpp
        imu_sample.hpp
        imu_config.hpp
        imu_registry.hpp
        mock_imu.hpp
    src/
      sensor_manager.cpp
      imu_registry.cpp
      mock_imu.cpp
    tests/
      sensor_manager_test.cpp
      mock_imu_test.cpp

services/
  sensor/
    imu_publisher/
      CMakeLists.txt
      main.cpp

examples/
  sensor/
    imu_reader/
      CMakeLists.txt
      main.cpp
```

外部或独立 package：

```text
openember-pkg-imu-dm/
openember-pkg-imu-bosch/
openember-pkg-imu-invensense/
```

不再使用旧式：

```text
modules/sensor/...
```

当前 OpenEmber 主干架构中，`modules/` 不作为主干层级。

## 6. CMake 与 Kconfig

建议 CMake target 命名保持当前工程风格：

```text
openember_sensor
openember::sensor
```

如果后续拆分较细，再考虑：

```text
openember_sensor_core
openember_sensor_imu
openember::sensor_core
openember::sensor_imu
```

V1 推荐先保持一个组件 target：

```text
openember_sensor
```

Kconfig 建议放在：

```text
Components Layer
  Sensor Framework
```

建议配置：

```text
config OPENEMBER_COMPONENT_SENSOR
    bool "Build component: Sensor Framework"
    default y

config OPENEMBER_SENSOR_IMU
    bool "Enable IMU sensor interfaces"
    default y
    depends on OPENEMBER_COMPONENT_SENSOR

config OPENEMBER_SENSOR_MOCK_IMU
    bool "Build Mock IMU"
    default y
    depends on OPENEMBER_SENSOR_IMU
```

采样发布服务属于 Application Layer：

```text
Application Layer
  Services
    Sensor
      IMU publisher
```

传感器诊断工具也属于 Application Layer，不属于 `components/sensor`：

```text
Application Layer
  Tools
    Sensor tools
```

外部真实传感器 package 的源码来源应后续放到 Third party / bundles 或产品工程自己的构建配置中，不应硬塞进 `components/sensor`。

## 7. 基础类型

当前 OpenEmber 还没有一个跨组件通用的 `Result<T>` 或 `Timestamp` 类型。`components/transport` 内部已有 `transport::Result`，但它是 transport 领域类型，不应直接复用到 Sensor Framework。

V1 建议在 `components/sensor` 内定义轻量基础类型：

```cpp
namespace openember::sensor {

enum class SensorType {
    kUnknown,
    kImu,
    kGnss,
    kTemperature,
    kHumidity,
    kPressure,
    kEncoder,
    kForce,
    kWeight,
};

enum class SensorBusType {
    kUnknown,
    kI2c,
    kSpi,
    kUart,
    kCan,
    kOneWire,
    kModbus,
    kVirtual,
};

enum class SensorFetchMode {
    kPolling,
    kEvent,
    kFifo,
};

enum class SensorError {
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
    kInternalError,
};

struct Error {
    SensorError code = SensorError::kNone;
    std::string message;
};

template <typename T>
class Result;

template <>
class Result<void>;

}
```

原则：

- 先在 sensor 组件内闭环。
- 不引入 C++23 `std::expected` 作为强依赖。
- 不复用 transport 专用错误。
- 如果未来 OpenEmber 引入通用 `openember::Result<T>`，再统一迁移。

## 8. 生命周期接口

V1 生命周期保持简单：

```cpp
namespace openember::sensor {

enum class SensorState {
    kStopped,
    kRunning,
    kError,
};

struct SensorInfo {
    std::string name;
    SensorType type = SensorType::kUnknown;
    std::string driver;
    std::string vendor;
    std::string model;
    std::string frame_id;
    SensorBusType bus_type = SensorBusType::kUnknown;
    std::chrono::microseconds min_period{0};
    std::size_t fifo_depth = 0;
    std::vector<SensorFetchMode> supported_fetch_modes;
};

struct SensorStatus {
    SensorState state = SensorState::kStopped;
    SensorError last_error = SensorError::kNone;
    std::string message;
};

class ISensor {
public:
    virtual ~ISensor() = default;

    virtual Result<void> Start() = 0;
    virtual Result<void> Stop() = 0;

    [[nodiscard]] virtual SensorStatus Status() const = 0;
    [[nodiscard]] virtual const SensorInfo& Info() const = 0;
};

}
```

生命周期：

```text
create
  -> Stopped
  -> Start()
  -> Running
  -> Stop()
  -> Stopped
```

错误：

```text
Running
  -> Error
```

约束：

- `Stop()` 必须幂等。
- 析构必须释放资源。
- 析构不得抛异常。
- `Start()` 失败后对象仍应处于可析构、可查询状态。
- 不可恢复错误进入 `kError` 后，是否允许再次 `Start()` 由具体 driver 决定，但必须明确返回结果。

## 9. 类型化传感器接口

Sensor Framework 不提供万能数据接口。不同传感器类型定义自己的接口：

```cpp
namespace openember::sensor {

class IImu : public ISensor {
public:
    virtual Result<ImuSample> Read(std::chrono::milliseconds timeout) = 0;
};

}
```

未来可以扩展：

```text
IGnss
ITemperatureSensor
IPressureSensor
IEncoder
```

原则：

```text
生命周期统一；
数据接口类型化；
硬件差异在 package 层结束。
```

## 10. IMU V1 标准数据

V1 重点支持六轴 IMU：

```cpp
namespace openember::sensor {

struct Vector3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

enum class SensorTimeSource {
    kUnknown,
    kDevice,
    kHostMonotonicOnReceive,
};

struct SensorTimestamp {
    std::uint64_t monotonic_ns = 0;
    SensorTimeSource source = SensorTimeSource::kUnknown;
};

struct ImuSample {
    std::uint64_t sequence = 0;
    SensorTimestamp timestamp;
    Vector3d acceleration_mps2;
    Vector3d angular_velocity_radps;
    std::optional<double> temperature_celsius;
};

}
```

标准单位：

| 字段 | 单位 |
|------|------|
| `acceleration_mps2` | m/s^2 |
| `angular_velocity_radps` | rad/s |
| `temperature_celsius` | deg C |
| `timestamp.monotonic_ns` | ns |

`frame_id` 不放进热路径 `ImuSample`，默认从 `SensorInfo::frame_id` 或 publisher 配置中取得。发布到 openember-msgs 时，publisher 可以把 `frame_id` 写入消息。

V1 暂不包含：

- orientation
- covariance
- magnetic field
- calibrated/un-calibrated 双通道
- 多 IMU 硬件同步

这些等真实产品需要后再扩展。

## 11. 时间戳原则

传感器内部采样时间优先使用单调时间，不默认使用墙上时钟。

优先级：

```text
设备硬件时间戳
  -> 同步后的设备时间戳
  -> driver 收到完整数据帧时的 host monotonic 时间
```

原因：

- `CLOCK_REALTIME` 可能被系统校时影响。
- 高频采样需要单调递增时间。
- 控制和滤波通常关注时间间隔。

但是跨进程消息发布仍可能需要 Unix time。建议后续在 openember-msgs 的 sensor 消息中同时表达：

```text
Header.timestamp_unix_ns   发布或封包时间
sample_time_monotonic_ns   采样时间，来自 producer 本机单调时钟
time_source                时间来源
```

跨设备严格同步、PTP、PPS、GPS Clock 不属于 V1。

## 12. 数据获取模型

V1 使用阻塞式 Pull API：

```cpp
auto result = imu.Read(std::chrono::milliseconds{100});
```

语义：

- 成功返回一个完整 `ImuSample`。
- 没有数据时等待到超时。
- 超时返回 `SensorError::kTimeout`。
- 设备断开返回 `SensorError::kDisconnected` 或 `SensorError::kIoError`。
- 不允许无限期阻塞。

线程约束：

- V1 默认只支持一个 reader 线程调用 `Read()`。
- `Start()` / `Stop()` 可由控制线程调用。
- `Stop()` 必须能让阻塞中的 `Read()` 尽快返回。
- 如果 package 内部使用后台线程或厂商 SDK 线程，package 必须保证 `Stop()` 和析构能安全结束线程。

缓冲策略：

- Sensor Framework 不强制全局 ring buffer。
- 如果 driver/package 需要内部队列，必须在 package 文档中明确队列大小和溢出策略。
- IMU 高频数据默认建议丢旧帧保留最新帧，除非产品明确要求完整记录。

V1 不提供：

- callback API
- async API
- coroutine API
- Rx observable
- 自动采样线程

采样线程属于 `imu_publisher` 或具体应用。

### 12.1 FetchMode 语义

`SensorFetchMode` 描述传感器或 package 的数据获取能力：

| 模式 | 语义 | 典型来源 |
|------|------|----------|
| `kPolling` | 主动读取或按固定周期查询 | I2C/SPI register read、定时查询 |
| `kEvent` | 有数据事件后读取 | GPIO event、串口可读、CAN frame、IIO event |
| `kFifo` | 设备或内核缓冲区中批量取样 | IMU FIFO、IIO buffer、driver queue |

V1 的 `IImu::Read(timeout)` 不要求调用方知道底层模式。调用方只关心是否在超时时间内得到一个完整样本。

内部约束：

- `kPolling` package 必须控制最小查询间隔，避免 busy loop。
- `kEvent` package 必须保证 `Stop()` 能唤醒阻塞中的等待。
- `kFifo` package 必须明确溢出策略，推荐高频实时数据默认丢旧保新。
- package 可以支持多个 fetch mode，但默认模式应由配置或驱动自身选择。

## 13. SensorManager

V1 可以提供一个简单生命周期容器：

```cpp
namespace openember::sensor {

class SensorManager {
public:
    Result<void> Add(std::string name, std::unique_ptr<ISensor> sensor);
    ISensor* Find(std::string_view name);
    Result<void> StartAll();
    Result<void> StopAll();
    std::vector<SensorStatus> StatusAll() const;
};

}
```

职责：

- 保存 sensor 实例。
- 按名称查询。
- 批量启动。
- 批量停止。
- 查询状态。

不负责：

- 创建采集线程。
- 发布 middleware topic。
- 数据缓存。
- 自动重连策略。
- 姿态融合或滤波。

## 14. IMU Registry

V1 使用显式 registry，不使用全局静态构造自动注册。

```cpp
namespace openember::sensor {

struct ImuConfig {
    std::string name;
    std::string frame_id;
    std::map<std::string, std::string> values;
};

using ImuFactory =
    std::function<Result<std::unique_ptr<IImu>>(const ImuConfig&)>;

class ImuRegistry {
public:
    Result<void> Register(std::string driver_id, ImuFactory factory);
    Result<std::unique_ptr<IImu>> Create(std::string_view driver_id,
                                         const ImuConfig& config) const;
};

}
```

Package 显式注册：

```cpp
void RegisterMockImu(openember::sensor::ImuRegistry& registry);
void RegisterDmImu(openember::sensor::ImuRegistry& registry);
```

应用或服务启动时：

```cpp
openember::sensor::ImuRegistry registry;
RegisterMockImu(registry);
RegisterDmImu(registry);
```

这样启动路径确定、可测试、可裁剪，也避免静态初始化顺序问题。

## 15. Sensor Package

Sensor Package 是硬件扩展边界。

示例：

```text
openember-pkg-imu-dm/
  CMakeLists.txt
  include/openember/packages/imu/dm/dm_imu_l1.hpp
  src/
    dm_imu_l1.cpp
    protocol.cpp
    register.cpp
  tests/
    protocol_test.cpp
    recorded_stream_test.cpp
    mock_serial_test.cpp
```

Package 负责：

- 打开具体设备。
- 初始化设备。
- 解析协议。
- 校验帧。
- 处理半包、粘包、错误帧头、CRC 错误。
- 厂商单位转换为 OpenEmber 标准单位。
- 厂商错误码转换为 `SensorError`。
- 输出 `ImuSample`。
- 对复合传感器芯片共享 bus、fd、锁、event source、parser 或 register map。

Package 不负责：

- 创建 OpenEmber node。
- 发布 Link topic。
- 设备全局注册。
- Web Console。
- 数据录制。
- 姿态融合。
- 产品业务策略。

## 16. 配置模型

Sensor Framework V1 不绑定 config_service，也不直接读取全局配置文件。

推荐由 service/app 读取配置，再构造 `ImuConfig`：

```yaml
sensors:
  imu0:
    type: imu
    driver: mock_imu
    enabled: true
    frame_id: imu_link
    config:
      rate_hz: 200
```

Sensor Framework 只理解公共字段：

```text
name
type
driver
enabled
frame_id
```

`config:` 下的具体字段交给 package：

```yaml
sensors:
  imu0:
    type: imu
    driver: dm_imu_l1
    enabled: true
    frame_id: imu_link
    config:
      device: /dev/ttyUSB0
      baudrate: 921600
      output_rate_hz: 200
```

`baudrate`、`CAN ID`、`SPI mode`、寄存器配置等都不是 Sensor Framework 的公共语义。

后续 `config_service` 稳定后，`imu_publisher` 可以从参数服务读取配置，但这属于 service 逻辑，不属于 `components/sensor`。

## 17. openember-msgs 关系

`components/sensor` 使用 C++ struct，不直接依赖 Protobuf。

语言无关消息应放在 `openember-msgs`：

```text
openember.msgs.sensor.v1.ImuSample
openember.msgs.sensor.v1.SensorStatus
```

建议职责：

```text
components/sensor
  ImuSample C++ struct

openember-msgs
  sensor/v1 Protobuf message

services/sensor/imu_publisher
  C++ struct -> Protobuf
  Publish via OpenEmber Link
```

这样同一个 driver 可以用于：

- 单元测试
- CLI 读取工具
- 生产运行时 publisher
- 硬件诊断工具
- 未来数据录制工具

而不会被 middleware 绑定。

## 18. ImuPublisher Service

采样与发布应放在 Application Layer 的 service 中：

```text
services/sensor/imu_publisher
```

职责：

- 初始化 OpenEmber Link client。
- 读取 sensor 配置。
- 注册需要的 IMU package。
- 创建 `IImu`。
- 启动采样线程。
- 调用 `IImu::Read(timeout)`。
- 转换为 openember-msgs sensor 消息。
- 发布 topic。
- 发布自身 `NodeInfo` / `NodeHeartbeat`。
- 必要时向 `device_manager` 暴露设备状态。

不负责：

- 解析厂商协议。
- 实现滤波/EKF。
- 决定产品行为。

推荐 topic：

```text
/sensors/imu/<name>/sample
/diagnostics/imu_publisher
```

具体 key 仍由 OpenEmber Link 的 KeyBuilder 添加 robot_id 和 namespace 前缀。

## 19. Mock IMU

V1 必须内置 Mock IMU，放在 `components/sensor` 或 `components/sensor/imu` 内。

默认输出：

```cpp
acceleration_mps2 = {0.0, 0.0, 9.80665};
angular_velocity_radps = {0.0, 0.0, 0.0};
temperature_celsius = 25.0;
```

配置：

```yaml
config:
  rate_hz: 200
```

用途：

- 验证 `IImu`。
- 验证 `ImuRegistry`。
- 验证 `SensorManager`。
- 验证 `imu_reader`。
- 验证 `imu_publisher`。
- 在 CI 中无硬件运行。

Mock IMU 是 V1 的第一优先级，因为它能先把 framework、service、message、runtime 路径跑通。

## 20. 可靠性要求

V1 必须满足：

1. `Start()` / `Stop()` 可重复调用。
2. `Stop()` 和析构不会泄漏资源。
3. `Read(timeout)` 不允许永久阻塞。
4. 设备断开不导致进程崩溃。
5. 协议 parser 必须防御异常输入。
6. 热路径避免不必要堆分配。
7. 不使用隐藏线程，除非 package 文档明确说明。
8. 不使用全局静态自动注册。
9. 不把厂商错误码泄漏给上层业务。

热路径建议：

- 使用固定大小 buffer。
- 使用 `std::array` / `std::span`。
- 避免每帧构造大量字符串。
- `ImuSample` 保持固定小对象。

## 21. 测试要求

Sensor Framework 测试：

- `SensorManager` add/find/start/stop。
- `ImuRegistry` register/create/error path。
- `MockImu` start/read/stop/timeout。
- `Read()` 在 `Stop()` 后能返回。

Sensor Package 测试：

- Protocol parser valid frame。
- Invalid checksum。
- Partial frame。
- Multiple frames。
- Random bytes。
- Timeout。
- Device disconnect。
- Repeated start/stop。

真实硬件测试不进入普通 CI，但应提供硬件测试入口：

```text
imu_hardware_test
```

## 22. V1 实施顺序

推荐按以下顺序实现：

1. 在 `components/Kconfig` 增加 Sensor Framework 配置。
2. 新增 `components/sensor` CMake target。
3. 实现基础类型：`SensorError`、`Result`、`SensorInfo`、`SensorStatus`、`SensorTimestamp`。
4. 实现 `ISensor`。
5. 实现 `IImu`、`ImuSample`、`ImuRegistry`。
6. 实现 `MockImu`。
7. 添加 `components/sensor` 单元测试。
8. 添加 `examples/sensor/imu_reader`。
9. 在 `openember-msgs` 增加 `sensor/v1` 消息。
10. 添加 `services/sensor/imu_publisher`。
11. 让 `imu_publisher` 发布 `NodeInfo` / `NodeHeartbeat` / IMU sample。
12. 后续接入 `device_manager`，暴露 IMU 的 `DeviceInfo` / `DeviceState`。
13. 再实现第一个真实硬件 package，例如 DM-IMU-L1。

## 23. V1 验收标准

V1 完成标准：

- `components/sensor` 不依赖 `core`。
- `components/sensor` 不包含任何厂商协议。
- Mock IMU 可在无硬件环境运行。
- `IImu& imu = ...; imu.Read(100ms);` 与具体硬件无关。
- 替换 IMU package 不需要修改 `imu_publisher` 主逻辑。
- `imu_reader` 可以读取 Mock IMU。
- `imu_publisher` 可以发布 Mock IMU 数据。
- 设备异常、超时、错误帧不会导致 OpenEmber runtime 崩溃。
- `device_manager` 和 Sensor Framework 的职责边界保持清晰。

## 24. 暂缓功能

以下功能不进入 V1：

- 动态插件加载。
- Package Manager。
- 自动远程 package registry。
- 热插拔自动重建。
- 自动故障恢复策略。
- 配置 schema 自动生成。
- Sensor Graph。
- 统一异步 API。
- Callback API。
- Coroutine API。
- 多传感器硬同步。
- PTP/PPS 时间同步。
- IMU Fusion。
- Camera、LiDAR、Radar。
- 零拷贝大数据框架。

## 25. 结论

OpenEmber Sensor Framework V1 的定位是：

```text
一个位于 Components Layer 的轻量、类型安全、可测试的传感器接入层。
```

第一阶段只解决一件事：

```text
让不同 IMU package 能通过同一个 IImu / ImuSample API 被 OpenEmber 使用。
```

最终关系：

```text
platform/lpio
  -> Sensor Package
  -> components/sensor
  -> services/sensor/imu_publisher
  -> OpenEmber Link / openember-msgs
  -> product app / tools / web_console

system/device_manager
  负责运行时设备视图和查询，不承载具体传感器协议。
```

这个设计避免把底层协议、运行时节点、消息发布和业务行为揉在一起。先把 Mock IMU 到 ImuPublisher 的最小链路做稳，再根据真实硬件和产品需求逐步扩展。
