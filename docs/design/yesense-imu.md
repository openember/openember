# YESENSE YIS321 与统一设备配置

## 分层与可选依赖

`sensor::IImu` 是厂商无关的采样接口。`components/sensor/drivers/yesense` 提供 `sensor::YesenseImu`，目标为 `openember::yesense_imu`；HI 内置 `imu/yesense_readonly` 工厂，通过 `MakeImuEndpoint` 发布公共 `openember.msgs.sensor.v1.ImuSample`。产品只定义实例配置，无需再注册厂商工厂。

Kconfig 的 `OPENEMBER_SENSOR_YESENSE` 默认关闭，依赖 Sensor Framework；需要 Linux 和 `OPENEMBER_ENABLE_LPIO=ON`。串口传输使用核心 `lpio::SerialPort`，帧解析由 Yesense 驱动自行实现，无需外部 IMU SDK。要通过 HI 使用，还须启用 `OPENEMBER_SERVICE_HARDWARE_INTERFACE` 和 `OPENEMBER_HARDWARE_ENDPOINT_IMU`。

## device.yaml

HI 的 `LoadConfigFromFile` 同时支持旧的 `hardware_interface/endpoints` 配置和下面的新格式。`devices` 是按实例名索引的映射，可以逐步容纳不同外设；实例名同时作为 endpoint_id 和 sensor_id。文件在启动时读取，修改后重启节点，无隐式查找、文件合并或热更新。

```yaml
schema_version: 1
robot_id: DEMO_ROBOT
devices:
  imu0:
    type: imu
    driver: yesense_readonly
    model: YIS321
    enabled: true
    mode: readonly
    frame_id: yesense_sensor_unverified
    topic: /sensors/imu/imu0/sample
    critical: true
    connection:
      device: /dev/imu_sensor
      baud_rate: 921600
      rx_poll_wait_ms: 5
```

`robot_id`、`schema_version: 1`、设备 `type` 必填；Yesense 还要求显式的型号、readonly 模式、绝对串口路径和支持的非零波特率。轮询等待范围为 1–10 ms，以保留停止取消上界；它不是设备采样周期。允许的主机波特率为 9600、19200、38400、57600、115200、76800、230400、460800、921600。未知字段、重复键、非法波特率、非有限/非正发布频率会在加载时拒绝。禁用实例不会打开串口。驱动未编译时，不会回退为 Mock，启用端点启动失败。

`publish_rate_hz` 沿用 HI 端点字段，但通用 IImu worker 按成功的新读取发布，不借此配置设备输出频率，也不做精确限速。可用已有 `openember_hardware_interface --config device.yaml` 加载（连接系统 Link）；产品也可调用同一加载器和 `BuiltinEndpointRegistry` 创建自己的节点。

## 采样与姿态边界

默认只读配置设置的是主机 UART，不发送 IMU 查询、设置、校准或持久化命令。使用 flock 和 TIOCEXCL 约束串口持有者，停止时取消读取并恢复原 termios2；这些机制不能排除启动前已有的非协作持有者。`YesenseImu(std::string device)` 保留旧诊断行为，使用当前主机波特率；新装配使用 `YesenseImuOptions` 的显式配置。

驱动校验设备 TID 进展、有限值及四元数范数，重复/损坏样本不会刷新接收时间。加速度单位 m/s²、角速度 rad/s（驱动将协议中的 deg/s 转换为 rad/s）。`receive_time_monotonic_ns` 是主机接收时间；设备时间尚未完成时钟域对齐，`sample_time_monotonic_ns` 保持 0，clock_domain 按采集会话隔离。

当前只完成原始只读链路。型号标识不等于已核验固件、参考坐标系和安装方向；`orientation_valid=false`、参考系和安装版本标记未核验，管理面保留 Warning。设置 `frame_id` 只命名原始传感器坐标系，不会转换数据。安装变换、姿态参考系核验、时延分位告警和真实运控融合仍需后续完成，目前没有这些 YAML 字段，不能写入一个“单位安装矩阵”来绕过核验。

关节、策略、动作仍分别维护自己的参数文件；简单外设集中在 `device.yaml`。设备连接参数不进入 Kconfig，真实设备配置也不应自动用于仿真发布源。

## 原生协议解析

`src/yesense_decoder.hpp` 实现 0x59/0x53 帧同步、TID、长度、CK1/CK2 校验及 TLV 边界检查。按当前帧实际包含的加速度（0x10）、角速度（0x20）、wxyz 四元数（0x41）、温度（0x01）或磁场强度（0x31）交付样本，各字段独立标记有效性，不跨帧拼接；未知 TLV 按长度跳过。重复字段、错误长度与校验失败帧不交付。支持拆包、粘包和噪声后重新同步，接收缓存最多 262 字节；单次读取最多排空 64 KiB，超限报错。设备采样时间字段暂不映射到公共单调时钟。

默认只读会话不调用串口写操作或发送查询、校准、配置命令。Start 清空旧接收数据并设置独占，Stop 恢复原始主机串口参数，包括自定义速度。公共接口及 device.yaml 字段保持不变。

## 可选设备控制

Yesense 实现 IMU 可选配置与校准接口，默认关闭设备控制，HI 的只读工厂保持原行为。独立 CLI 可通过直接连接的 `--device-control` 启用设备查询、输出率/字段/波特率设置及静态陀螺校准。采样与应答共享解析入口，RAM 为默认持久化方式；Flash 需显式选择。完整事务语义、能力边界和后续 Web/Link 集成方案见 [IMU 子框架](imu-framework.md)。
