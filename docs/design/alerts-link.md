# 告警事件与 Link

告警与日志使用两条独立输出路径：`alerts → logging → spdlog TopicSink → 日志 topic` 用于排查；`alerts → LinkPublisher → 结构化告警 topic` 用于整机状态管理、界面和运维程序订阅。`TopicSink` 只发布日志，不承担告警协议。检测、整机响应和执行仲裁继续分开，订阅者不能通过日志等级推导固定停机动作。

## 构建与协议

启用 `OPENEMBER_COMPONENT_ALERTS_LINK` 后提供目标 `openember::alerts_link` 和头文件 `<openember/alerts/link_publisher.hpp>`，依赖 `OPENEMBER_COMPONENT_ALERTS`、Link 和 `openember-msgs` 的 `alerts/v1` 契约。核心默认关闭这个可选适配，四足 host 产品显式开启。缺少消息能力时配置直接报错，需使用包含 `OPENEMBER_ALERTS_CONTRACT_VERSION=1` 的消息仓库；无需为纯进程内告警引入 Link 或 protobuf 依赖。

公共协议位于 `openember-msgs/proto/openember/msgs/alerts/v1/alerts.proto`：`AlertEvent` 表示一次被接受的条件变化或提醒，`AlertState` 表示一个发布实例的完整状态快照。协议使用 protobuf，经 OpenEmber `Node` 的类型发布订阅接口传输，独立于 JSON 日志 topic。

| 逻辑 topic | 消息类型 | 内容 |
| --- | --- | --- |
| `/alerts/events` | `openember.msgs.alerts.v1.AlertEvent` | 触发、提醒、解除、未知事件及采样元数据 |
| `/alerts/state` | `openember.msgs.alerts.v1.AlertState` | 完整条件表、修订号、心跳、在线标记和派发统计 |
| `/openember/log` | 现有 JSON 日志 | warning/error 文本记录，由 logging TopicSink 发布 |

前两个 topic 属于传入 Node 的通信范围。默认空 namespace 时，robot_id 为 `P05W_C0_HW1D0` 的底层 key 分别为 `openember/P05W_C0_HW1D0/topics/alerts/events` 和 `openember/P05W_C0_HW1D0/topics/alerts/state`。订阅端必须使用相同 robot_id 和 namespace；`LinkOptions.robot_id` 还用于协议 Header，调用方应确保与 Context 一致。已有日志 TopicSink 使用自身独立的 Context、robot_id `openember` 和固定本机连接配置，不会因为增加告警适配而自动迁移其范围。

## 派发与生命周期

Manager 接受事件时，同步维护日志队列、有界事件历史和状态表，没有网络或日志 I/O。事件历史默认128项，由 `Options.history_capacity` 配置，独立于日志队列；`ReadSince(revision)` 在短时互斥锁内读取一致的增量历史、当前状态、统计和修订号。每次接受事件递增全局 revision，`ForgetSession` 真正移除状态时也递增 revision，但不伪造解除事件。快照包含所有最后观测，包括已解除条件；`REMINDER` 表示仍成立，`UNKNOWN` 不能视为健康。

`LinkPublisher` 使用自己的线程，默认每20 ms检查历史、每1 s发布完整快照，状态变化时也立即发布快照。它不调用日志 sink，不通过 `Options.observer` 接入，因此日志写入、日志 topic 和日志 observer 的阻塞不会阻塞已接受事件的 Link 派发。日志队列或状态表满时 Manager 仍可拒绝新的提交，被拒绝的事件不会进入任一输出通道；必须监控拒绝计数。

构造适配时传入活着的 Manager、Node 和 LinkOptions，通信资源创建失败则构造失败。停止顺序为：停止事件生产者 → `LinkPublisher::Stop()` → `Manager::Stop()` → 关闭 Context。适配停止前再读取一次当前状态并尝试发布 `online=false`，析构也会 Stop。Manager、Node及Context必须在适配存续期间有效；不要从发布线程回调销毁它们。Link 的发送失败只计数，不改变控制权限；快照周期继续尝试发送。

```cpp
openember::alerts::Manager alerts;
// context 已 Init，runtime.robot_id 与 link_options.robot_id 一致。
auto node = std::make_shared<openember::Node>(context, "motion_alerts");
openember::alerts::LinkOptions link_options;
link_options.robot_id = "robot-1";
openember::alerts::LinkPublisher publisher(alerts, node, link_options);
// 检测模块调用 alerts.Publish(event)，日志和 Link 各自派发。
// 停止检测模块后：
publisher.Stop();
alerts.Stop();
```

## 整机状态管理的消费规则

每个适配生命周期产生新的 `publisher_instance`；同一实例内 revision 单调递增，事件的采样序号仍是原检测条件的 sequence，两者不能混用。Header 的 Unix 时间是网络消息生成时间，`observation.sample_time_ns` 属于明确的 `clock_domain`；仿真暂停不会推进来源时间，但 Link 心跳仍按主机时间运行。发布者心跳只证明状态发布端在线，不证明每个检测器仍在采样；消费者还需结合来源采样时间、检测器生命周期和运动模式判断数据有效性。

消费者按 robot、发布实例和条件 key 管理数据。状态快照只替换所属发布实例的条件集合，包括空集合，不能清空其他实例的告警。旧 revision 的快照不得覆盖较新的状态；同一 revision 的新心跳可刷新在线时间。事件 revision 不大于已应用快照时，可保留为审计记录，但不能重新改变当前状态。重复事件按实例与 revision 去重，未知枚举按无法判断处理。

新订阅者或重连订阅者等待下一次周期快照恢复当前状态，不依赖此前的边沿事件。不同 topic 之间不假定接收顺序；快照可覆盖尚未收到的事件。`heartbeat_sequence` 单调递增，默认 `stale_after_ms=3000`，要求至少为快照周期三倍。超过接收端本机单调时钟超时或收到 `online=false` 时，将该来源标记为不可用/未知，不能解释为告警全部解除。进程异常退出依靠心跳超时，正常退出的最后一条离线状态也可能丢失。

历史溢出后适配继续发送尚存事件和完整快照，并累计 `history_gaps`；`publish_failures` 是本地发送失败计数，不能证明远端已经收到。协议不提供持久化事件、应用确认、断线重放或 exactly-once 保证；当前状态可以通过周期快照恢复，丢失的历史事件可能无法恢复。统计中的 `rejected_events` 来自 Manager，不能将零个 active 条件与事件链路完全健康混为一谈。

## 四足接入与测试

`quadruped_sim_remote` 为 `quadruped.policy` 的姿态事件创建 Manager 与独立 LinkPublisher，使用机型 robot_id。默认 Link peer 模式发现同范围订阅端；可通过 `--link-connect tcp/127.0.0.1:7447` 显式接入已有 router。Link 初始化失败会记录 warning 并保留本地控制和日志，本次运行不会自动重建失败的适配。R只更换控制会话并移除旧会话状态，发布实例保持不变；运行器退出时尝试发布离线状态。

订阅诊断工具是 `openember_alert_monitor`，在启用告警 Link 后可显式构建；核心关闭普通工具构建时该目标仍可单独构建，不加入默认 ALL。

```bash
cmake --build build/quadruped-host --target quadruped_sim_remote openember_alert_monitor -j4
./build/quadruped-host/bin/openember_alert_monitor --robot P05W_C0_HW1D0
```

工具打印 `ALERT_EVENT` 和周期 `ALERT_STATE`，后者包括条件总数、active、unknown、心跳与派发统计；已观察来源心跳过期时打印 `ALERT_STALE ... state=unknown`。显式连接 router 时，仿真器与工具均添加对应连接参数，工具使用 `--connect`。工具是只读观察入口，不会创建控制权或请求动作。未来整机管理模块应使用类型订阅和上述消费规则，结合任务、动作阶段与其他反馈决定响应。

核心测试覆盖真实 router/client 发布订阅、全部事件类型及元数据、会话移除、离线快照、晚订阅恢复、发布实例重启、有界历史溢出，以及日志派发阻塞时的独立 Link 派发。协议仓库提供 `alerts_wire_contract` 序列化与未知字段测试。四足保留姿态事件、停止、暂停、控制故障等仿真回归；具体行为仍是 report_only，未增加翻身起身或整机状态管理服务。
