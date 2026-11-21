# Sensor CLI

`openember_sensor_cli` 是 Sensor 框架的交互诊断工具。第一期提供 `imu` 子命令，支持 Mock 和可选 Yesense 驱动；采集依赖 `IImu`，CLI 不处理厂商报文，也不依赖 HI、Link 或产品代码。它直接持有设备，运行前应停止使用同一串口的其他进程。

## 构建

在 `ember menuconfig` 中启用 Tools → Build interactive sensor diagnostic CLI，需要同时启用 Sensor Framework、YAML 配置组件和 Tools。构建开关为 `OPENEMBER_ENABLE_TOOLS_SENSOR_CLI`，真实 Yesense 还需要 `OPENEMBER_SENSOR_YESENSE` 与 LPIO。通过产品 `ember build` 构建时，产品应将 `openember_sensor_cli` 加入自身构建目标依赖；也可以在已配置的目录执行 `cmake --build <build-dir> --target openember_sensor_cli`。程序输出到 `<build-dir>/bin/`。

仅 Mock 构建无需真实驱动。`drivers` 命令列出当前编译进来的驱动，不自动下载、扫描或选择设备。

## 启动

```bash
# 无需连接硬件，采样 200 Hz。
openember_sensor_cli imu --driver mock --rate 200

# 明确指定串口及主机波特率，不向设备发送配置命令。
openember_sensor_cli imu --driver yesense --device /dev/imu_sensor --baud 921600

# 使用 schema_version: 1 的统一外设配置，选择一个启用的 IMU。
openember_sensor_cli imu --config device.yaml --sensor yesense0
```

配置文件格式与 HI 的 `device.yaml` 外设描述一致；当前只加载指定实例，其他外设不会打开。`yesense_readonly` 驱动标识映射到 Yesense，要求 `mode: readonly`、`model: YIS321` 和显式连接参数。配置文件不能与 `--driver / --device / --baud / --rate / --frame-id` 混合覆盖。`--sensor` 在直接连接时命名实例，在配置文件模式下选择实例。`--rate` 是 Mock 的模拟采样率；`topic / critical` 为整机集成元数据，本工具不发布消息。真实设备的 `publish_rate_hz` 不设置硬件输出率；Mock 使用该值作为模拟采样率。

默认启动时立即打开设备。`--closed` 仅创建诊断会话，等待用户输入 `open`；不打开串口。该模式下仍会检查驱动是否存在和连接配置是否受支持。

## 交互命令

| 命令 | 行为 |
| --- | --- |
| `help` / `?` | 帮助 |
| `drivers` | 当前可用驱动 |
| `info` | 驱动报告的实例、型号、厂商、坐标系元数据，不代表已查询固件身份 |
| `config` | 主机连接配置 |
| `status` | 是否采集中、交付样本数、读超时、最近接收年龄、设备健康状态及错误 |
| `diagnostics` | 可选驱动计数器，未实现此能力时显示 `supported=false` |
| `sample [--timeout 1]` | 等待一帧新样本，超时单位为秒 |
| `stream [--hz 10] [--seconds 5]` | 连续显示最新样本；省略时长则持续至 Ctrl+C |
| `stats [--seconds 10]` | 每秒显示交付频率，结束时汇总接收间隔和驱动诊断 |
| `record --file "imu samples.jsonl" [--seconds 10]` | 记录全部交付样本；省略时长则持续至 Ctrl+C，不覆盖已有文件 |
| `close` | 停止采集并释放设备 |
| `open` / `reopen` | 创建新采集会话并打开设备，统计清零 |
| `quit` / `exit` | 停止并退出 |

后台持续读取，提示符等待输入时也不会停止排空串口。`stream --hz` 只控制显示频率，不改变实际采样率。`sample` 等待新的交付样本，`stream` 在显示间隔内只展示最新值，不声称显示了全部帧。

`Ctrl+C` 结束连续显示、统计和记录，返回提示符；在提示符处按 Ctrl+C、输入 EOF 或 `quit` 退出程序。SIGTERM 清理资源后退出。设备 I/O 错误会停止采集并保留错误信息，使用 `status` 查看，修复连接后显式 `reopen`，不自动重连。批处理命令失败返回非零状态；交互中的命令错误会显示 `ERROR` 并继续接受输入。

## 数据及统计语义

样本以 JSON 显示，记录为每行一个 JSON 对象，含传感器/坐标系、时钟域、序号、采样与接收时间、各字段有效性和 SI 单位数据。不可用的加速度、角速度、磁场和温度值为 `null`。四元数以 `raw_quaternion_wxyz` 保留原始值，同时携带 `orientation_valid`；未核验姿态参考系时不得将其用作控制姿态。设备时间未知时 `sample_time_monotonic_ns=0`，不会由主机接收时间填充。

`delivered_hz` 为本工具成功读取的样本数除以实际测量时长，不等同于硬件采样频率或线路解码频率；一次排空多帧时驱动可以只交付最新帧。`read_timeouts` 是本工具 50 ms 读取期限内无新样本的次数，低输出频率设备可以正常出现超时。平均接收间隔基于同一时钟域内有效的主机接收时间；最大接收间隔为当前采集会话累计最大值，并非本次 stats 窗口的最大值。驱动计数器单独输出，不能与 CLI 计数混淆。

记录通过最多 4096 个样本的队列与采集解耦，文件写入不持有采集锁。存储速度不足导致队列溢出时报告错误，记录不完整，不静默丢帧；采集继续运行。记录包含驱动交付样本，不包含驱动内部合并前的所有线路帧。文件只在记录结束时检查刷新结果；不要将该工具当作掉电可靠的数据记录系统。

默认保持只读采集。设备配置和校准需显式启用，见下方；当前没有远程 Link 订阅或远程配置功能。

## 自动化使用和回归

```bash
openember_sensor_cli imu --driver mock \
  --command sample \
  --command "stats --seconds 1" \
  --command quit

python3 tools/sensor-cli/test/cli_test.py <build-dir>/bin/openember_sensor_cli
# 构建包含 Yesense 时追加 --yesense，增加 PTY 检查，不操作真实串口。
```

可重复使用 `--command` 按顺序执行命令，适合脚本和 CI；也可将交互命令通过标准输入传入。自动化应为 stream/record 指定有限时长。

## 驱动扩展

`openember::sensor_drivers` 提供 `ImuDriverRegistry`，通过类型明确的 `ImuDriverConfig` 创建 `IImu`。内置驱动装配与基础 Sensor 库分开，避免基础接口与厂商驱动循环依赖。新增驱动可注册工厂，在内置装配处关联构建选项；CLI 的采集、显示、统计和记录无需修改。当前没有运行时动态库插件加载。

`ISensorDiagnostics` 是可选能力接口，提供命名计数器；Yesense 实现字节、解码、交付、TID/数值拒绝、超时和排空超限计数。IMU 子框架已经提供独立的可选配置和校准接口，驱动统一处理命令响应与流式报文，CLI 不打开第二个协议读取线程。

## IMU 配置与校准

```bash
# 台架设备控制，明确选择直接连接；不要与 HI 或其他串口 owner 同时运行。
openember_sensor_cli imu --driver yesense --device /dev/imu_sensor \
  --baud 921600 --device-control
```

```text
capabilities
device-config
set rate 100
set fields accel,gyro,quaternion
set fields accel,gyro,quaternion,temperature,euler,timestamps ram
set rate 400 flash
set baud 460800 ram
calibrate static-gyro ram
```

`capabilities` 只读取驱动能力元数据；`device-config` 才向设备查询实际值。`config` 显示主机连接配置，两者不同。Mock 支持内存配置和模拟校准，没有 UART/Flash；可以不加 `--device-control` 直接体验命令。Yesense 的查询也会发送协议命令，因此默认只读模式下拒绝查询和写入。只读 device.yaml 不能与 `--device-control` 混用，台架设置使用上面的显式直接连接方式。

输出内容支持 `accel,gyro,quaternion,temperature,magnetic,euler,timestamps`，按能力选择，不把厂商位掩码传入 CLI。`none` 关闭输出；之后 `sample` 会超时，但配置查询仍可使用。只输出欧拉角或设备时间时，当前没有可交付的公共测量字段；这些字段不会被伪装成已验证姿态或单调设备采样时间。缺少四元数时 `orientation_present=false`、原始四元数显示 null；有原始四元数但参考系未核验时 present=true、valid=false。

设置默认 RAM，Flash 必须写出后缀。设置结果逐字段显示 APPLIED、REJECTED、UNCONFIRMED；已应用的前置字段不会因后续失败自动回滚。输出率/内容的成功需 ACK 与回读相符；波特率没有写入 ACK，需切换主机速度并验证新连接。Flash 请求的 ACK 和 RAM 回读不能独立证明掉电后配置仍存在。查询超时后不要自动重复写入，先检查状态并显式关闭重开。

波特率未确认时可执行 `close`、`host-baud 921600`、`open`，明确指定期望的主机速度重连。`host-baud` 仅修改关闭会话的下一次连接参数，不发送设备设置。Stop 恢复主机原始串口参数，但不会恢复设备原来的设置；设置成功后需明确更新之后启动使用的 YAML。

校准要求连接至少 10 秒并有持续静止观测，发送后仍需保持静止至少 1 秒。Yesense 返回 ACCEPTED 表示设备接受请求，并非校准质量验收或安装标定完成。Mock 返回 COMPLETED 只是模拟结果。当前没有异步校准任务进度服务。

协议事务和整机配置管理边界见 [IMU 子框架](../../docs/design/imu-framework.md)。离线配置回归使用 `python3 tools/sensor-cli/test/configuration_test.py <build-dir>/bin/openember_sensor_cli`，只操作 PTY 模拟设备，包含 RAM/Flash、混合报文、读回不一致、超时、带宽和校准条件检查。
