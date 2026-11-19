# 通用告警组件

`components/alerts` 提供进程内告警状态、事件队列和异步日志派发，目标为 `openember::alerts`，头文件为 `<openember/alerts/manager.hpp>`。通过 Kconfig/CMake 的 `OPENEMBER_COMPONENT_ALERTS` 选择是否编译，默认开启。检测算法、阈值滞回和去抖、持续提醒周期由检测模块负责；告警组件不获取控制权，不执行停机、锁存或恢复动作。

## 事件与状态

`Event.key` 用来源模块、设备、会话和告警代码共同标识条件。`Raised`、`Reminder`、`Cleared`、`Unknown` 分别记录条件成立、持续提醒、解除和无法判断。严重程度独立选择 `Severity::Warning` 或 `Severity::Error`，不从严重程度推导整机处理动作。`Snapshot()` 返回每个条件最后一次接受的事件；`Reminder` 表示条件仍成立，`Unknown` 不能视为正常。快照保留原始采样时间，消费者自己检查新鲜度；停止检测不会自动宣称条件解除。

事件记录采样序号、来源时间和时钟域、持续时间、测量值、阈值、解除阈值、单位、参考系、上下文和说明。相同键内只接受递增序号、非倒退时间和相同的时钟域，不混算主机时间与仿真时间。新会话必须更换 session；停止旧生产者后使用 `ForgetSession(source, session)` 释放旧状态，它不会伪造解除事件，已经排队的历史事件仍会写日志。组件不代替生产者判断哪个会话已失效，旧生产者必须由调用方停止。

字段均限制为256字节并拒绝控制字符，关键标识和时钟域非空；数值必须有限，无效观测使用 `Unknown`、`valid=false` 和有限的占位值。日志使用带引号的字段，避免说明文字伪造新的日志行。进程内类型通过可选 Link 适配映射到 `openember-msgs` 的 `alerts/v1` 协议；尚无应用确认、持久化状态或集中告警服务。

## 与日志结合

每个已接受事件由派发线程调用现有 `openember::logging::Logger`，spdlog 后端继续复用标准输出、滚动文件、syslog 和日志 topic 的已有实现；选择 builtin 后端时写 stderr。所有生命周期事件都至少用 `warn`，严重事件用 `error`；解除事件同样用 `warn` 并标记 `edge=CLEARED`，因此不能只数 warning 日志行来判断当前未解除告警。

管理器持有独立日志实例，将该实例及其 topic 的过滤阈值设为 `warn`，避免普通日志的 error-only 配置过滤告警；不修改普通进程日志等级或替换默认 spdlog 实例。名称追加进程内实例编号，例如 `quadruped-sim-alerts-1`，滚动文件名沿用此名称。输出目的地、文件目录和轮转参数继承传入的 `LoggerConfig`。文件创建失败等底层行为沿用日志模块的回退机制，不保证落盘或远端送达，topic 的限流和可用性也仍然适用。

队列与状态表默认各128项，容量可在创建 `Options` 时指定，上限65536项。`Publish()` 只做校验、短时互斥锁、元数据复制与入队，没有日志 I/O，不等待慢日志或 observer；它仍涉及分配和互斥锁，不是硬实时接口。队列或状态表满时返回 `Full`，不改变快照；非法数据、重复/旧事件、关闭后提交分别返回 `Invalid`、`Stale`、`Stopped`。所有拒绝进入计数，运行中的派发线程用 warning 汇总拒绝数。调用方应检查返回值，或监控 `Stats()`，不能把拒绝当成成功上报。停止后提交只计数，派发线程已经结束。

`Options.observer` 在日志调用之后、派发线程上接收事件，用于本地订阅；独立 Link 适配使用有界事件历史，不依赖此回调。回调应在有限时间内返回，不能在回调里调用同一管理器的 `Stop()` 或析构管理器。回调异常被隔离并计入 `dispatch_errors`，不会结束派发线程。`Stats().delivered` 是完成派发尝试的数量，不是存储或网络确认；`dispatch_errors` 记录向调用方抛出的日志/回调异常，底层日志内部处理的错误仍按日志模块规则处理。

## 用法与生命周期

```cpp
openember::alerts::Options options;
options.logging = openember::logging::LoggerConfig::from_build_defaults("robot-alerts");
openember::alerts::Manager alerts(std::move(options));
openember::alerts::Event event;
event.key = {"motion", "robot-1", "session-1", "tilt"};
event.sequence = 1;
event.time_ns = 1000000000;
event.clock_domain = "simulation";
event.value = 0.8;
event.threshold = 0.7;
event.clear_threshold = 0.6;
event.unit = "rad";
event.context = "locomotion";
const auto result = alerts.Publish(std::move(event));
// 消费 result / Stats()，产品独立决定运动响应。
// 先停生产者，再在控制循环之外排空队列并结束线程。
alerts.Stop();
```

管理器构造完成后启动派发线程，日志后端无法初始化则构造失败。`Stop()` 关闭接收、排空已接受事件并 join，支持重复调用，析构时也会调用。慢日志或回调可能延长排空时间，所以停止和析构必须在控制线程之外执行。第一版提供有界的进程内派发，不承诺进程崩溃或断电后保留队列。

## 四足产品接入

四足产品的纯阈值检测器仍属于 `quadruped_policy`，根据 YAML 产生倾角与低根高度事件。运行器将其映射到 `openember::alerts::Event`，来源为 `quadruped.policy`，保留仿真时间、会话、单位与动作上下文，交给核心日志线程处理。窗口仍显示 `TILT WARNING` 或 `LOW HEIGHT WARNING`，产品当前明确使用 `report_only`。R 重置时停止旧控制会话、移除旧会话告警快照并创建新会话；告警不影响独立物理步进。

## 验证

核心测试 `alerts_lifecycle`、`alerts_validation`、`alerts_capacity`、`alerts_async` 覆盖生命周期、采样顺序、时钟域、无效数据、容量、慢消费者、回调异常和关闭排空；spdlog 后端增加 `alerts_logging`，检查实际 warning/error 文件记录、解除标记和采样元数据，并验证 error-only 输入配置不会吞掉 warning 告警。

结构化告警的独立发布、topic、心跳与整机状态管理消费规则见[告警事件与 Link](alerts-link.md)。
