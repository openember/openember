# Actuator v2 控制校验与状态机

`components/actuator` 提供 `openember::actuator::control::Guard` 及独立域类型，对应 `openember-msgs` 的 [actuator/v2 契约](../../../openember-msgs/docs/actuator-control-contract.md)。它不依赖 Protobuf、网络、MuJoCo 或电机 SDK，不改变旧 `JointCommand`、MockJointController 或 HI v1 端点。

当前实现是单执行上下文使用的控制门控组件，已通过 FakeBackend 离线行为测试。现已提供 [v2 Protobuf 适配及 HI owner 会话](actuator-control-adapter.md)，网络端点工厂和真实设备驱动仍待实现，现有仿真器没有启用它。域类型及消息转换显式校验枚举，禁止把未知值默认映射为合法模式。

## 接口与执行顺序

公开头文件为 `openember/actuator/control_guard.hpp`，链接 `openember::actuator`。

- `ValidateCapabilities` 检查版本、身份/字符串上限、时钟域前缀、完整拓扑与有限限值，Guard 构造时执行，失败抛 `invalid_argument`。
- `Control` 同步执行管理请求，直接返回 COMPLETED/REJECTED/FAILED，不存在异步 ACCEPTED。结果不包含 wire 层的请求关联字段；HI 适配器须从原请求填入这些字段。
- `Accept` 原子验证完整关节目标，仅保留最新一条，不写后端。无效请求不能覆盖旧有效目标、推进有效序号或刷新 watchdog。
- `RejectInput` 供消息适配器计入格式错误的拒绝诊断，同时推进 Tick，不更新已接受目标或 watchdog。
- `Apply` 执行前调用 Tick，重新核对会话、期限、租约与状态，确认后端仍 Ready 后同步写出最新目标，成功才推进 applied 序号；允许在 I/O 周期内重复应用仍有效的同一目标。后端失去就绪状态会锁存故障。
- `Tick` 必须在每个 I/O 周期调用，包括无新消息时。租约、当前目标期限或目标 watchdog 的任一到期都会清除目标、撤销租约、调用禁用并锁存 FAULT；主机时钟为零/倒退同样锁存。
- `Snapshot` 为诊断副本，不包含令牌。`backend_failure` 锁存后端操作失败，`fault_reason` 提供期限/时钟/后端等故障原因。失败的禁用不能证明设备已停止。
- `SetPaused` 仅仿真可用，进入暂停撤权并禁用，恢复不自动 ARM。暂停时允许获取控制权，但禁止 ARM/目标；租约继续按主机时间到期。
- `Reset` 更换 session、撤权并清空目标、序号及请求缓存，恢复 DISARMED；无效或与当前相同的 session 会锁存故障。调用方必须保证 session 从未复用，并同步复位后端/物理世界；若世界仍暂停，要在处理新消息前同步 SetPaused。
- `Stop` 撤权并调用禁用，当前 session 进入 STOPPED；析构再次 Stop，要求后端禁用操作幂等。重新启用须建立新 session。

所有方法（包括 Snapshot）由同一个 owner 串行调用。网络回调只能向 owner 的有界入口提交数据，不能并发调用 Guard。组件本身没有线程或独立 watchdog；如果 owner 卡住，Tick 不会运行，因此后端仍需独立的超时保护。字符串、vector/map 使用动态内存，当前不宣称硬实时或无分配。

ARM 只代表软件状态允许目标；构造时先禁用，获取控制权不使能，ARM 后首个有效目标实际 Apply 前继续禁用。ARM 后没有目标也从 ARM 时刻开始计超时。后端 Ready 返回 false 则拒绝 ARM。

## 后端与授权边界

注入的 `Backend` 提供 Ready、Apply、Disable、EmergencyStop、ClearFault，均须有界、同步、noexcept 且不能重入 Guard。Backend 生命周期必须覆盖 Guard 及其析构。

后端 Apply 负责最终 PD/模式计算、关节和 actuator 变换、力矩限幅与非有限结果拒绝，不能持有传入目标的引用。Guard 验证目标字段及输入上限，不计算动力学或电机 SDK 帧；连续轴 MIT 的展开坐标语义须由后端一致实现。Disable 必须走实际禁用路径，不能用一次零力矩写出冒充。返回 false 时组件清目标、撤权并锁存故障；软件无法替代已失败的硬件停止路径。

调用方须从本机 CLOCK_MONOTONIC 取得 now_ns，并从本机启动标识构造 clock_domain；组件只比较配置与输入，不能从消息证明远端来自同机。跨主机和离线加速网络控制仍未支持。

`Permission` 由接入层的认证/授权结果传入，默认全部禁止：control 控制获取/续租/释放/ARM；stop 控制 DISARM/急停；clear_fault 单独控制复位。不能从生产者名称或消息字段推导权限。周期目标仍须先经过接入层限制，再凭当前生产者与 generation/token 验证所有权。

TokenFactory 由 owner 注入，必须提供 1–63 字节、不可预测且每次新生成的 ASCII token；组件检查长度和字符，不提供随机性证明。生成器缺失则构造失败，抛异常/返回非法 token 则获取失败并锁存故障。离线测试使用固定序列生成器，不可用于网络部署。

## 去重和有界状态

目标序号与租约管理序号独立，获取请求和安全管理请求分别按 producer_session 追踪。安全调用者的高序号不会污染持有者序号。有效请求以 `(producer_session, request_id)` 缓存完整请求和结果，相同请求重试返回历史结果，不再次续租/使能；改变内容的同 ID 请求拒绝。历史结果中的 state/lease 到期时间是原执行结果，当前状态以 Snapshot 为准。

最多缓存 128 个仍有效的管理请求、128 个 producer/流的重放游标。请求过期后可以清除结果，游标在整个 session 内保留，不能淘汰后让旧序号重新生效。容量耗尽时不继续分配或驱逐有效记录，而是禁用并进入 STOPPED，返回 FAILED/BUSY；包括停止请求触发容量耗尽时也尝试禁用。恢复须新 session。HI 仍须限制消息字节数、并发/排队容量和生产者来源；这不是完整的抗滥用机制。

未通过上下文、授权、基本形状或重放校验的请求直接拒绝；未执行任何副作用，无幂等缓存保证。通过这些入口校验后的状态拒绝及执行结果会缓存。语义完整相同但字段顺序不同的 wire 数据应由适配器转换成相同域请求。

`SampleFresh` 仅判定单关节样本序号、接收时间、fresh 标识和最大年龄，不修改样本。逐通道有效性、反馈序号停更/倒退追踪、聚合反馈组装和反馈失效联锁仍需后续适配器实现；不能只靠此函数宣布整机反馈健康。

## 离线测试

依赖 CMake 和 C++17，无需 OpenEmber 外部依赖或设备：

```bash
cmake -S components/actuator/test -B build/actuator-control -DCMAKE_CXX_FLAGS='-Wall -Wextra -Werror'
cmake --build build/actuator-control
ctest --test-dir build/actuator-control --output-on-failure
```

核心 `TESTS_ENABLED=ON` 且 actuator 组件开启时也会注册相同用例。12 组用例覆盖能力/32 关节上限、目标形状与限值、会话/时钟、租约互斥/更替、幂等、watchdog、accepted/applied 区分、后端失败/权限/故障锁存、暂停/reset、缓存/游标容量/时间溢出、单轴新鲜度与时钟倒退。测试以显式时间推进，不依赖 sleep。

消息适配、后端所有权注入、[单 worker 调度和优先停止](actuator-control-loop.md) 已接入产品 MuJoCo。清故障请求必须晚于本次故障锁存时刻，不能通过旧请求解除新的故障。后续补齐网络端点、授权、独立客户端和 IMU/策略输入联锁；完成之前不能给现有端点声明 `actuator.control.v2`。
