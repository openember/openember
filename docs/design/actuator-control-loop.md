# Actuator v2 单 worker 调度

`joint_control_loop.hpp` 提供 `JointControlLoop`，在独立 worker 上创建 Session 和产品 Backend，串行执行管理、Tick、反馈读取/仿真步进及 Apply。后端创建、操作、禁用与销毁均在该 worker，外部线程只提交消息或读取状态副本。

这是进程内 API，没有注册 topic/RPC。目标属性 `OPENEMBER_ACTUATOR_V2_CONTROL_LOOP=1` 说明构建包含此 API，不代表网络端点已支持 v2。

## 输入和停止

- 普通 FIFO 最多 32 项，每周期最多处理 4 项；命令和普通管理请求共享容量。队列满立即返回失败，不刷新 watchdog。
- DISARM/EMERGENCY_STOP 使用独立的 8 项优先队列，唤醒 worker，并在普通任务之间及 Apply 前处理。入口检查 stop 权限、身份/会话/模型、时钟和期限，执行时 Guard 再校验序号、幂等及状态。
- 有效停止完成或执行失败后取消普通队列，完成其 future，避免排队的旧清故障/获取控制权随后执行。Guard 还要求 CLEAR_FAULT 的 issued_time 严格晚于故障锁存时刻，旧请求返回 REPLAY。
- 已授权且上下文有效的停止使优先队列溢出时，终止当前 session。返回失败，不冒充原操作完成；无效/旧会话停止不能利用溢出触发终止。
- `RequestStop()` 使用原子标志唤醒 worker，不依赖队列容量。`Stop()` 由外部 owner 调用并 join，不能在 Backend/read/clock/token 回调中调用；回调可以调用 RequestStop。

停止是协作式的，可以越过待处理任务，不能中断正在执行的后端调用。所有回调和后端操作必须有界，真实设备仍需独立 watchdog。该线程不是硬实时线程，没有承诺最大 OS 调度延迟。

输入为已解码 Protobuf，每条最多 16 KiB，checked 形状通过后才复制入队。网络层仍须在解码前限制字节数并认证/授权。入队不等于 Guard 接受或后端应用，调用者必须处理 future。入口格式/容量拒绝未送入 Guard，不计入其域命令拒绝计数。

## 周期与反馈

period 必须为正且小于 command_timeout。过载后从当前时刻安排下一周期，不补跑积压物理步；优先停止提前处理不会额外推进物理世界。

1. Tick 主机时钟，先撤销已到期控制。
2. read 回调读取一帧，可推进一次仿真。Frame 保留源 session/clock_domain、逐轴采样序号/时间及 paused，暂停变化同步到 Guard，不自动恢复 ARM。
3. 验证完整反馈，处理有界普通任务，优先检查停止。
4. ARMED 时所有轴必须有有效位置、速度和未过期样本，否则 TripFault(EXPIRED)，禁用并撤权。
5. 仍 ARMED 且有目标才 Apply，再发布反馈。

worker 只生成发布序号/时间，不生成采样时间。暂停/优先停止时可以发布旧样本，但不刷新其时间，fresh 会降级。目前检查有效性与年龄，尚未追踪“样本序号停更但来源伪造新接收时间”的跨帧异常，也未接 IMU/策略输入联锁。

后端/read/时钟回调异常会终止会话、尝试禁用、销毁后端并完成待处理 future；错误保留在 Error()。最后一帧若无法合法编码则清除缓存，避免保留旧 ARMED 状态。Start 只保证 Session/Backend 初始化成功，不保证已取得首帧。

循环是一次性对象。reset/重连/模型变化须先 Stop/join，再用新 session、能力及后端构造新循环；运行中来源 session 改变会终止，不自动重新标记样本。

## 验证与接入

沿用 HI 独立测试入口：

```bash
cmake -S services/hardware_interface/test/actuator_control -B build/actuator-v2-hi \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Werror'
cmake --build build/actuator-v2-hi
ctest --test-dir build/actuator-v2-hi --output-on-failure
```

新增 7 组 worker 用例与原 8 组适配测试共 15 组，覆盖线程归属/析构、普通队列满仍可停止、取消旧清故障、优先溢出、排队过期、停更联锁、初始化/read/clock 异常及并发入口。测试闸门最长自行释放，用于制造队列边界。

产品 MuJoCo Backend 和 `quadruped_sim_control_demo` 已采用此路径。后续接网络端点、有身份的授权、独立客户端、逐轴序号追踪和 viewer 共用控制路径。现有 viewer 的 H/D 本地调试不经此 worker，不能同时控制同一 World。
