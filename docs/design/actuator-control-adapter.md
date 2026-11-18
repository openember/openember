# Actuator v2 消息适配与 HI owner 会话

本切片提供公共 `actuator_control_adapter.hpp` 和 `joint_control_session.hpp`，把 v2 Protobuf 接到 [核心 Guard](actuator-control-guard.md)。`JointControlSession` 拥有产品注入的 Backend，通过单一 owner 串行调用；没有网络订阅、RPC 注册、工作线程或自动使能。现有 HI v1 与仿真器本地控制入口不变。

## 消息边界

`openember::services::hardware_interface::actuator_control` 提供 checked `FromProto`/`ToProto`，返回 `hardware::Result<T>`，失败没有可用值。

| 入口 | 转换与检查 |
|---|---|
| JointCommand → domain::Command | context/lease 必须存在；身份 ASCII/长度、非零序号、时间基本形状、1–32 关节、名称唯一、已知模式、所有目标数值有限。 |
| ControlRequest → domain::Request | 明确的非零操作、request_id、context；ARM/RENEW/RELEASE 必须有完整租约，其他操作租约为空；只有 ACQUIRE/RENEW 可以设置正 lease duration。 |
| ControllerCapabilities ↔ domain::Capabilities | 显式枚举映射，完整拓扑、模式集合、限值和版本校验，包含连续轴及逐轴反馈年龄上限。 |
| domain::ControlResult → ControlResult | 保留生产者/request_id/序号/操作关联；接收方权威身份来自当前能力；仅成功获取/续租能携带 token，拒绝/失败结果不能泄漏租约。 |
| domain::FeedbackFrame + Status → ControllerFeedback | 完整拓扑、frame 的 session/clock_domain 与当前能力相符，合法 owner/accepted/applied 关联，有限有效通道及逐轴时间。 |

不把未知枚举转换为合法默认，不截断身份，不补接收时间，不把缺失 lease 解释为当前持有者。转换只做形状检查；机器人/模型/时钟匹配、真实期限、模式所用字段、能力限值和控制权由 Guard 在收到/实际执行时检查。接入层必须先限制序列化消息的字节数，不能依赖解码后的数量检查限制 Protobuf 分配。

反馈域类型位于 `openember/actuator/control_feedback.hpp`。Frame 必须携带源 controller_session/clock_domain，避免 reset 后把旧样本改用新 session 发布。发布序号/时间由 owner 提供，逐关节 sample_sequence/receive_time 原样保留。接收 fresh 声明会按公布的 max_feedback_age 和 publish_time 再检查，过期时降为 false；不会更新样本时间或序号。

有效零值保留 valid=true；不可用温度/电压/故障等值输出为零、valid=false。有效通道 NaN/Inf、未来接收时间、无样本却声明有效、重复/缺失关节、真实后端携带仿真步数均拒绝整帧。仿真 step=0 时只允许无物理采样的初始反馈。状态只发布实际力矩，目标力矩不能由适配器冒充实际反馈。

`ControllerFeedback` 使用新字段 22 backend_failure、23 fault_reason、24 lease_expires_time_ns 保留核心状态信息；原字段号不变。无持有者时租约关联、目标游标及接受时间必须清零。旧绑定缺少这些字段的反馈不能据默认零推断停止已经成功。

## owner 会话

`JointControlSession(capabilities, unique_ptr<Backend>, tokens)` 转移后端所有权；空指针拒绝构造。成员销毁顺序保证 Guard 先执行 Stop，再销毁后端。

- `Control(message, now, permission)`：先 Tick，再 checked 转换和同步管理操作，最后产生带关联的结果。格式错误返回 Result failure；合法但不满足权限/状态等语义的请求返回 wire REJECTED；执行失败返回 wire FAILED。未来 RPC 端点必须传播格式错误，不能用空/默认结果当成功响应。
- `Command(message, now)`：checked 转换后交给 Guard。格式错误通过 `RejectInput` 计入拒绝诊断，仍运行 Tick；不覆盖有效目标或刷新 watchdog。
- `Apply(now)`、`Tick(now)`：由同一 owner 在每个 I/O 周期调用，不能仅靠消息到达推进 watchdog；只接受消息不会产生写出。
- `Capabilities()`、`Feedback(frame)`：返回 checked wire 数据；Feedback 不采样、不隐式 Tick，也不推进物理世界。owner 须先用当前主机时间 Tick，再组装当前状态的 frame。
- `SetPaused`、`Reset`、`Stop` 委托给 Guard。reset 后旧 frame/目标因 session 不同被拒绝；若世界保留暂停状态，须先同步 SetPaused 再处理新命令。

会话默认不授予任何权限，Permission 必须来自接入层，不能从消息声称的 producer_session 推导。token 生成仍由部署方注入安全生成器，测试生成器不可用于网络部署。本对象没有独立线程、安全回调队列或硬件 watchdog；这些属于下一步 HI owner 调度和后端集成。

## 构建与验证

链接 `openember::hardware_interface`。actuator 开启且 `openember::msgs_cpp` 宣告 `OPENEMBER_ACTUATOR_CONTRACT_VERSION>=2` 时编译本 API，并设置目标属性 `OPENEMBER_ACTUATOR_V2_MESSAGE_ADAPTER=1`；该属性不是网络端点的 v2 能力声明。旧消息依赖仍可构建 v1 HI，产品使用新头文件前应检查该属性并同步消息版本。

无需网络/设备即可独立验证实际序列化消息通过会话到 FakeBackend 再生成反馈：

```bash
cmake -S services/hardware_interface/test/actuator_control -B build/actuator-v2-hi \
  -DOPENEMBER_MSGS_SOURCE_DIR=/path/to/openember-msgs \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Werror'
cmake --build build/actuator-v2-hi
ctest --test-dir build/actuator-v2-hi --output-on-failure
```

默认使用核心旁的 `../openember-msgs`。核心 TESTS_ENABLED 开启且适配器可用时也注册同样测试。8 组测试覆盖消息检查、能力、结果关联/令牌隔离、逐关节新鲜度、完整 wire/owner/backend 闭环、无效消息不刷新 watchdog、过期队列、授权、暂停/reset 与析构顺序。

已提供 [单 worker、有界输入和优先停止](actuator-control-loop.md)，并接入产品 MuJoCo Backend；位置/速度有效性和年龄联锁已有实现。尚待完成：网络端点工厂、接入授权、独立硬件 watchdog、逐轴序号追踪、IMU/策略输入联锁与独立客户端测试，不能声明网络运控或实机运控已完成。
