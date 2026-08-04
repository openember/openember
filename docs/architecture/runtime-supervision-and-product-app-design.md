# OpenEmber Runtime Supervision 与 Product App 设计方案

本文档定义 OpenEmber 中 `launch_manager`、`health_monitor`、`device_manager`、`config_service`
与 product app 的职责边界，并说明 `smart_device_demo` 如何演化为真正的产品应用入口。

该设计面向智能设备、机器人、边缘网关和工业控制等场景。核心目标是让系统既能保持
OpenEmber 的轻量、清晰和可裁剪，又能支撑未来更复杂的整机行为、节点管理和运行时监督。

## 一、核心结论

OpenEmber 的系统级职责应拆成五类：

| 模块 | 核心职责 | 不负责 |
|------|----------|--------|
| `launch_manager` | 启动/停止/监督进程，维护 runtime alive 状态，作为默认 Link router | 业务行为决策、设备状态解释、复杂健康诊断 |
| `health_monitor` | 聚合节点心跳、诊断、系统资源健康，输出健康结论 | 启动子进程、直接控制硬件、执行业务策略 |
| `device_manager` | 管理硬件设备发现、状态、能力和设备侧生命周期 | 进程监督、整机行为决策 |
| `config_service` | 管理配置、参数、持久化和配置下发 | 进程监督、健康诊断、业务策略 |
| product app | 管理具体产品行为、整机状态机或行为树 | 作为系统 router、监督所有进程、聚合全局健康 |

推荐运行模型：

```text
launch_manager
  ├── Link router
  ├── process supervisor
  ├── launch config loader
  └── runtime control endpoint

system / services / product app
  ├── Link client
  ├── publish NodeInfo
  ├── publish NodeHeartbeat
  └── optional LifecycleCommand handler
```

一句话边界：

```text
launch_manager 管 runtime 是否活着；
health_monitor 管系统是否健康；
device_manager 管设备是否可用；
config_service 管配置是否正确；
product app 管产品应该做什么。
```

## 二、Router / Client 启动模型

OpenEmber Link 当前基于 Zenoh transport。为了让普通用户不需要理解底层通信细节，
默认采用如下约定：

```text
每个 OpenEmber runtime 实例默认 1 个 router
普通节点全部作为 client 接入该 router
```

在单设备上：

```text
launch_manager 监听 tcp/127.0.0.1:7447
其他节点连接 tcp/127.0.0.1:7447
```

在多设备或分布式系统中，可以扩展为：

```text
每台设备一个本地 launch_manager/router
设备之间通过 Zenoh 路由能力互联
上层节点仍然只看到 OpenEmber Link
```

默认不建议让每个节点自行选择 router/client。否则系统启动顺序、端口占用和故障恢复会变得混乱。

### 独立调试例外

示例或 product app 在开发阶段可以提供 `auto` 模式：

```text
先尝试 router；
如果本机已有 router，则退到 client。
```

但在正式产品中，product app 应固定作为 client，由 `launch_manager` 统一启动。

## 三、launch_manager 的职责边界

`launch_manager` 是 OpenEmber runtime 的入口，不是整机业务大脑。

它应该负责：

1. 创建系统唯一默认 Link router。
2. 读取 launch 配置。
3. 启动被管理进程。
4. 记录进程 pid、启动时间、退出码、重启次数。
5. 处理 `SIGINT` / `SIGTERM`，按顺序停止子进程。
6. 根据 restart policy 重启异常退出的进程。
7. 发布 runtime process event。
8. 提供 start / stop / restart / query 等 runtime control service。

它可以做的最小健康判断：

- 进程是否存在。
- 进程是否在启动窗口内注册或发送心跳。
- 被管理节点的心跳是否长时间消失。

但这些判断只用于 runtime supervision，例如标记进程 stale 或触发重启。
全局健康结论仍应由 `health_monitor` 输出。

它不应该负责：

- 判断“机器人是否应该继续巡航”。
- 判断“传感器数据是否可信”。
- 管理设备能力和硬件资源。
- 聚合复杂诊断项。
- 执行整机 FSM 或行为树。

### launch 配置模型

第一版建议使用 YAML 配置，后续也可以支持 JSON、TOML 或编译期静态配置。

示例：

```yaml
runtime:
  robot_id: openember
  namespace: ""
  link:
    mode: router
    listen: tcp/127.0.0.1:7447

nodes:
  - name: health_monitor
    kind: system
    command: ${openember.bin}/health_monitor
    args: []
    restart: always
    startup_timeout_ms: 3000
    shutdown_timeout_ms: 3000
    heartbeat_timeout_ms: 5000

  - name: device_manager
    kind: system
    command: ${openember.bin}/device_manager
    restart: always
    startup_timeout_ms: 3000
    heartbeat_timeout_ms: 5000

  - name: smart_device_app
    kind: application
    command: ${openember.bin}/openember_smart_device_demo
    args: ["--client"]
    restart: on-failure
    restart_limit: 3
    startup_timeout_ms: 5000
    heartbeat_timeout_ms: 5000
```

第一版可以先支持最小字段：

| 字段 | 说明 |
|------|------|
| `name` | 进程名，也是默认节点名 |
| `command` | 可执行文件路径 |
| `args` | 参数列表 |
| `working_directory` | 工作目录 |
| `env` | 环境变量 |
| `restart` | `never` / `on-failure` / `always` |
| `restart_limit` | 重启次数上限 |
| `startup_timeout_ms` | 启动超时 |
| `shutdown_timeout_ms` | 优雅停止超时 |
| `heartbeat_timeout_ms` | 心跳超时 |

当前第一阶段实现已经落地：

```bash
./build/bin/launch_manager --launch configs/smart_device.launch.yaml
./build/bin/launch_manager stop
```

当前 `launch_manager` 已移除旧版内置 SMM/FSM：

- 不再订阅 `/sys/module/register` 的旧模块注册消息。
- 不再维护旧的全局 `State context`。
- 不再发布 `/sys/state/post` 的旧 POD 状态消息。
- 不再提供旧 `smm_msg_t`、`state_msg_t`、`event_msg_t` 等 POD 消息 traits。
- 进程监督由 `ProcessManager` 和 launch 配置负责。
- 节点可观测性后续统一迁移到 openember-msgs 的 `NodeInfo`、`NodeHeartbeat`
  和 `ProcessInfo`。

默认 pid file 位于：

```text
/tmp/openember-launch_manager.pid
```

可以通过 `--pid-file <file>` 或环境变量 `OPENEMBER_PID_FILE` 覆盖。默认 launch 文件查找顺序：

1. `OPENEMBER_LAUNCH_FILE`
2. 当前工作目录下的 `configs/smart_device.launch.yaml`
3. 相对 `launch_manager` 可执行文件的源码树配置路径

当前默认配置只启动 `smart_device_app`，因为部分 system/service 节点还只是注册后退出的占位实现。
等这些节点 daemon 化并发布稳定心跳后，再把它们加入默认 launch 配置。

### launch_manager 内部结构

建议逐步重构为：

```text
LaunchConfig
  读取和校验 launch 文件

ProcessSpec
  一个被管理进程的静态配置

ManagedProcess
  一个进程实例的运行状态

ProcessManager
  fork/exec、waitpid、stop、restart

RuntimeRegistry
  记录进程、节点、最近心跳和 runtime 状态

LaunchManagerNode
  OpenEmber Link topic/service 接口
```

实现上优先使用 `fork` + `execve` / `posix_spawn`，避免 `system("cmd &")`：

- 参数和环境变量更清晰。
- pid 可准确记录。
- 不需要 shell。
- 停止、重启、waitpid 更可靠。

## 四、health_monitor 的职责边界

`health_monitor` 是系统健康聚合器。

它应该负责：

1. 订阅所有节点的 `NodeHeartbeat`。
2. 订阅所有节点或设备的 diagnostics。
3. 聚合 CPU、内存、磁盘、网络等系统资源状态。
4. 判断节点是否 stale。
5. 判断设备、服务、应用整体健康等级。
6. 发布系统健康快照。
7. 必要时向 `launch_manager` 请求重启某个进程。

它不应该直接 `kill` 或 `fork` 进程。进程控制统一走 `launch_manager`。

### 心跳归属

节点心跳的消费方可以有多个：

```text
launch_manager  关注被管理进程是否还活着
health_monitor  关注节点健康和全局健康结论
web_console     展示节点状态
tools           调试和观测
```

其中：

```text
health_monitor 是健康状态的主判断者；
launch_manager 只是 runtime restart policy 的执行者。
```

## 五、device_manager 的职责边界

`device_manager` 管理硬件设备，不管理业务流程。

它应该负责：

1. 发现设备。
2. 维护设备清单。
3. 发布 `DeviceInfo` / `DeviceState`。
4. 记录设备能力，例如 GPIO、PWM、I2C、SPI、CAN、Serial、RS485、SBUS 等。
5. 为上层节点提供设备查询和设备状态服务。

它不应该决定产品行为。例如：

```text
电池低电量时是否返航
电机故障时是否进入保护模式
传感器异常时是否暂停任务
```

这些决策应该由 product app 根据 `device_manager` 和 `health_monitor` 的状态做出。

## 六、config_service 的职责边界

`config_service` 管配置和参数。

它应该负责：

1. 提供参数读取、设置、订阅。
2. 管理 runtime 配置文件。
3. 管理 product app 配置。
4. 处理配置持久化。
5. 支持配置热更新策略。

它不应该直接启动进程或执行业务行为。

对于 product app：

```text
product app 读取配置；
config_service 保存配置；
launch_manager 决定是否因配置变化重启节点。
```

## 七、Product App 的职责边界

Product app 是产品行为入口。

前面创建的 `smart_device_demo` 就是 product app 的最小示例。后续它应演化为：

```text
smart_device_app
  ├── ProductContext
  ├── ProductStateMachine / BehaviorTree
  ├── RuntimeClient
  ├── HealthClient
  ├── DeviceClient
  ├── ConfigClient
  └── ProductHeartbeatPublisher
```

它应该负责：

1. 管理整机状态机或行为树。
2. 订阅系统健康状态。
3. 查询设备能力和设备状态。
4. 读取产品配置。
5. 调用 system/service 节点。
6. 发布产品级状态、任务状态和自身心跳。
7. 决定产品当前行为。

典型整机状态：

```text
Booting
Initializing
Idle
Ready
Running
Paused
Degraded
Recovering
Fault
ShuttingDown
```

典型行为决策：

```text
系统健康异常 -> 进入 Degraded 或 Fault
关键设备掉线 -> 停止任务并请求恢复
配置更新 -> 重新加载产品参数
用户启动任务 -> 进入 Running
用户暂停任务 -> 进入 Paused
```

这些都属于 product app，不属于 `launch_manager`。

## 八、FSM 与 Behavior Tree

OpenEmber 应同时支持两种产品行为组织方式：

| 方式 | 适用场景 | 推荐层级 |
|------|----------|----------|
| 简单 FSM | 智能设备、网关、状态数量少、流程确定 | product app 内置，不增加第三方依赖 |
| Behavior Tree | 机器人、Agent、复杂任务编排、可恢复/可并发行为 | 可选 component + product app 使用 |

### 第一阶段：内置轻量 FSM

`smart_device_demo` 第一版应使用简单 FSM：

```text
Booting -> Initializing -> Ready -> Running
                          ↓
                        Fault
                          ↓
                      Recovering
```

优点：

- 零第三方依赖。
- 容易理解。
- 适合最小 product app 示例。
- 适合嵌入式 Linux 上的基础产品。

### 第二阶段：可选 BehaviorTree.CPP

BehaviorTree.CPP 可以作为可选行为引擎引入，但不应成为 `core` 或 `launch_manager`
的默认依赖。

官方项目说明中，BehaviorTree.CPP 是 C++17 行为树库，主要面向机器人场景，也可用于
游戏 AI 或替代有限状态机；它支持异步 Action、响应式/并发行为、XML 运行时加载树、
自定义节点插件以及日志/分析能力。

项目地址：

```text
https://github.com/BehaviorTree/BehaviorTree.CPP
```

推荐集成方式：

```text
third_party/
  behaviortree_cpp archive or fetch source

components/behavior_tree/
  OpenEmber 轻封装

apps/smart_device_demo/
  可选行为树示例
```

Kconfig 可设计为：

```text
Components Layer
  Behavior
    Enable BehaviorTree.CPP

Application Layer
  Product Applications
    smart_device_demo
      Enable behavior tree demo
```

原则：

- BehaviorTree.CPP 只用于 product behavior，不进入 `launch_manager`。
- 简单设备默认仍使用 FSM。
- 复杂机器人/Agent 产品可启用 Behavior Tree。
- 行为树 XML 是 product 配置，不是 OpenEmber core 配置。

## 九、消息与通信接口

OpenEmber 已有 `openember-msgs`，其中可直接服务该设计：

| 协议域 | 用途 |
|--------|------|
| `node/v1 NodeInfo` | 节点注册、节点元信息 |
| `node/v1 NodeHeartbeat` | 节点心跳 |
| `runtime/v1 ProcessSpec` | 进程启动规格 |
| `runtime/v1 ProcessInfo` | 进程状态 |
| `runtime/v1 ProcessEvent` | 进程状态事件 |
| `runtime/v1 StartProcessRequest/Response` | 启动进程服务 |
| `runtime/v1 StopProcessRequest/Response` | 停止进程服务 |
| `diagnostics/v1 DiagnosticStatus` | 诊断状态 |
| `device/v1 DeviceInfo/DeviceState` | 设备信息和状态 |
| `parameter/v1` | 参数和配置交互 |
| `lifecycle/v1 LifecycleCommand` | 节点生命周期控制 |

建议 topic/service 命名：

```text
/runtime/process/events
/runtime/process/start
/runtime/process/stop
/runtime/process/restart
/nodes/<node>/info
/nodes/<node>/heartbeat
/diagnostics/<node>
/devices/<device>
/product/<product>/state
```

经过 `KeyBuilder` 后，实际 Zenoh key 会带上：

```text
openember/<robot_id>/<namespace>/topics/...
openember/<robot_id>/<namespace>/services/...
```

## 十、smart_device_demo 演进路线

当前 `smart_device_demo` 是 product app 的种子：

```text
初始化 Link
创建 smart_device_app 节点
发布 NodeHeartbeat
```

下一步应演化为真正 product app：

### Phase 1：Product App Skeleton

目标：

- 固定作为 Link client，由 `launch_manager` 启动。
- 发布 `NodeInfo`。
- 发布 `NodeHeartbeat`。
- 引入 `ProductState`。
- 引入最小 FSM。

目录建议：

```text
apps/smart_device_demo/
  CMakeLists.txt
  main.cpp
  product_context.hpp
  product_state.hpp
  product_controller.hpp
  heartbeat_publisher.hpp
```

### Phase 2：接入系统节点

目标：

- 从 `config_service` 读取产品配置。
- 从 `device_manager` 查询设备能力。
- 从 `health_monitor` 获取系统健康状态。
- 根据健康/设备/配置变化调整 product state。

### Phase 3：产品行为示例

目标：

- 实现一个最小但真实的行为流程。
- 例如：

```text
Booting
  -> LoadConfig
  -> WaitDevices
  -> Ready
  -> RunTask
  -> HandleFault
  -> Shutdown
```

### Phase 4：可选 Behavior Tree 示例

目标：

- 引入可选 BehaviorTree.CPP。
- 提供一个等价行为树 demo。
- 与 FSM demo 并存，展示简单产品和复杂机器人产品的两条路径。

## 十一、launch_manager 演进路线

### Phase 1：替换硬编码启动

当前 `launch_manager` 使用硬编码 `system("/opt/openember/bin/xxx &")`。
第一步应替换为配置驱动：

- 读取 launch YAML。
- 使用 `fork/exec` 或 `posix_spawn`。
- 记录 pid。
- `waitpid` 监听退出。
- SIGTERM 优雅停止，超时后 SIGKILL。

### Phase 2：发布 ProcessEvent

当前已落地：`launch_manager` 在进程 starting、running、stopping、exited、failed
等状态变化时发布 `ProcessEvent`。
调试时可以运行 `openember_runtime_process_listener` 订阅该 topic。

启动、退出、重启、停止时发布：

```text
/runtime/process/events
openember.msgs.runtime.v1.ProcessEvent
```

web_console、logger、health_monitor 可以订阅。

### Phase 3：提供 Runtime Service

提供：

```text
/runtime/process/start
/runtime/process/stop
/runtime/process/restart
/runtime/process/query
```

请求和响应使用 `runtime/v1` 消息。

### Phase 4：接入心跳监督

当前已部分落地：`launch_manager` 订阅 `/nodes/*/heartbeat`，根据 launch
配置中的 `startup_timeout_ms` 和 `heartbeat_timeout_ms` 监督被管理进程是否
按期发布 `NodeHeartbeat`。`smart_device_demo` 已改为发布到
`/nodes/smart_device_app/heartbeat`。

`launch_manager` 记录被管理进程对应节点的最近心跳时间：

- 启动后超过 `startup_timeout_ms` 未出现心跳 -> startup failed。
- 运行中超过 `heartbeat_timeout_ms` 未出现心跳 -> stale。
- 根据 restart policy 决定是否重启。

这仍然是 runtime alive 监督，不是系统健康聚合。

### Phase 5：systemd 集成

在 Linux 产品中，可由 systemd 启动 `launch_manager`：

```text
systemd -> launch_manager -> OpenEmber nodes
```

systemd 只看 `launch_manager`，OpenEmber 内部节点由 `launch_manager` 管理。

## 十二、避免的反模式

### 反模式 1：launch_manager 变成业务大脑

错误：

```text
launch_manager 判断机器人应该导航、返航、避障、充电
```

正确：

```text
product app 判断业务行为
launch_manager 只负责 product app 是否运行
```

### 反模式 2：health_monitor 直接杀进程

错误：

```text
health_monitor kill -9 unhealthy_node
```

正确：

```text
health_monitor 发布健康结论或请求 launch_manager 重启节点
launch_manager 执行进程控制
```

### 反模式 3：device_manager 承担业务策略

错误：

```text
device_manager 发现电机异常后决定整机进入 Fault
```

正确：

```text
device_manager 发布电机异常
health_monitor 聚合健康状态
product app 决定整机行为
```

### 反模式 4：所有节点都能当 router

错误：

```text
每个节点 auto router/client，谁先启动谁当 router
```

正确：

```text
正式系统中 launch_manager 是默认 router
其他节点固定 client
auto 模式只用于开发调试
```

## 十三、推荐实施顺序

建议按以下顺序推进：

1. 保持当前 `launch_manager` 作为默认 Link router。
2. 将 `smart_device_demo` 在正式 launch 配置中固定为 client。
3. 为 `launch_manager` 引入 launch YAML。
4. 用 `fork/exec` 或 `posix_spawn` 替换 `system("cmd &")`。
5. 引入 `ProcessManager` 与 `ManagedProcess`。
6. 发布 `runtime/v1 ProcessEvent`。
7. 让系统节点和 product app 发布 `NodeInfo` 与 `NodeHeartbeat`。
8. 让 `health_monitor` 聚合心跳和 diagnostics。
9. 将 `smart_device_demo` 演化为 FSM product app。
10. 评估并可选集成 BehaviorTree.CPP。

这个顺序能先建立稳定 runtime，再逐步加健康和业务行为，避免一开始就把所有复杂度塞进
`launch_manager`。

## 十四、最终目标

最终用户的典型体验应是：

```bash
./build/bin/launch_manager --launch configs/smart_device.launch.yaml
```

然后 OpenEmber 自动完成：

```text
启动 Link router
启动 system nodes
启动 services
启动 product app
监督进程状态
聚合健康状态
暴露 web_console / tools 观测入口
由 product app 管理整机行为
```

用户不需要关心 router/client，也不需要手动按顺序启动多个节点。
