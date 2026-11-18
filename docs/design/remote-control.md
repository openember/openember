# 遥控输入与动作映射

首版提供 `openember::remote_control` 组件，公共头文件为
`openember/remote_control/gamepad.hpp`。不依赖 Link、Protobuf、运控、HAL 或机器人型号。
`OPENEMBER_COMPONENT_REMOTE_CONTROL` 控制构建；源码嵌入的产品通过
`target_link_libraries(app PRIVATE openember::remote_control)` 使用。

## 分层与调用

- `GamepadDevice` 是输入接口，返回统一的 `GamepadState`。
- `F710Evdev` 是首个具体设备实现，使用 Linux evdev，只读打开、不独占设备。
- `InputMapper` 处理回中准入、输入有效性、死区、比例缩放和组合键上升沿。
- 产品初始化 `Mapping`，解释动作名称与单位，负责状态机、控制权仲裁和动作执行。

最小用法：

```cpp
namespace rc = openember::remote_control;
rc::Mapping mapping;
mapping.deadman = rc::Button::LB;
mapping.buttons = {{"request_action", {rc::Button::A}, true}};
mapping.axes = {{"forward", rc::Axis::LeftY, -0.3}};
rc::F710Evdev device;  // 或传入稳定的 event-joystick 路径
rc::InputMapper mapper(mapping);
// 在单一 owner 周期内调用：
auto state = device.Poll();
auto input = mapper.Update(state, std::chrono::steady_clock::now());
// 产品处理 available / ready / deadman / reset，再解释 actions 和 axes。
```

组件没有自己的线程。建议由产品输入循环每 10 ms 调用一次，单次最多读取 256 个事件，
重试连接间隔 1 s。设备对象、映射对象及诊断查询均由同一线程持有。
周期值是开发起点，不构成硬实时保证。慢动作/RPC 不应阻塞输入循环。

## F710 XInput

设备要求 `046d:c21f`，并校验所需按钮和轴的能力与范围。当前不接受 DirectInput、
其他 Xbox 兼容设备或 `js0` 接口。不引入 libevdev 依赖；直接使用 Linux input ioctl。
实现遇到 `SYN_DROPPED` 时关闭并重新连接，不使用丢失同步前的状态继续控制。

默认扫描 `/dev/input/event*`；只发现一个可读的匹配设备才连接。
多个匹配设备时拒绝猜测，要求指定路径。部署优先使用 `/dev/input/by-id/*-event-joystick`；
不将当前机器的 `event16` 当作固定编号。设备读取失败、挂断或事件预算耗尽会输出断连和零状态。

统一轴排列：`LeftX LeftY RightX RightY LeftTrigger RightTrigger HatX HatY`。
摇杆/方向键范围 `[-1, 1]`，扳机 `[0, 1]`；摇杆保留 Linux 原始方向，即上推 Y 为负。
统一按钮排列：`A B X Y LB RB Back Start Guide LeftStick RightStick`。
轴缩放采用死区外连续重映射，默认死区 0.12，避免中心偏移产生运动意图。

轴状态在 `SYN_REPORT` 后提交，半个事件包不暴露给产品。首次打开通过 ioctl 读取当前状态，
连接时已经按住按钮不会直接发出动作。映射按每次采样后的状态计算边沿，不保证捕捉
在两次 Poll 之间完成的极短按下/释放；目前不提供原始事件订阅、长按或震动输出。

## 输入有效性与执行边界

每次成功打开产生新的 generation。启动、断连恢复、generation 改变或采样过期后，
必须先看到全部按钮释放、摇杆/方向键/扳机回到中立区，才能再次输出动作。
默认采样最大年龄 200 ms；非有限轴值、越界值、未来时间和空 generation 均拒绝。
`sampled_at` 与 `Update(now)` 必须使用同一个主机 steady_clock。

持续轴动作仅在 `ready && deadman` 时有值，其他情况仍返回已配置轴名称但数值为零。
按钮动作默认要求 deadman；停止类动作可显式设置 `require_deadman=false`。
组合键按全部条件从假到真的边沿触发一次。先按动作键再补按 deadman 不会补发单键动作。
没有连接/回中准入时，不发出按钮动作。

`reset` 表示已可用的输入消失或会话改变，产品必须清除待执行意图；
deadman 释放不等同 `reset`，产品应另外处理 `deadman` 下降沿。
`actions` 是意图，不是已完成动作。产品消费时停止/解除使能优先于使能/姿态请求，
不得把同一快照中的冲突请求顺序执行成停止后又使能。

没有新 evdev 事件通常只是操作者没动，不能据此判断无线断连；接收器节点仍存在也不能证明
手柄仍在线。`sampled_at` 是本机观察时间，**不是无线心跳**。本组件只检测内核暴露的设备
故障和调用链停更；F710 关机、超出范围、拔电池的实际行为仍需现场验证。
在没有可靠的无线失联证据前，不应把该状态作为真实机器人的无线安全链路。

输入不直接获取 HI 租约、不自动 ARM、不刷新电机 watchdog，也不创建网络入口。
产品通过运控状态机把意图转换为关节目标，再调用已有 HI 保护链路。
后续增加其他遥控器实现时继续复用统一状态；多来源控制权仲裁与产品运动模式分开实现。

## 工具与验证

Linux 下启用 `OPENEMBER_ENABLE_TOOLS` 和 `OPENEMBER_ENABLE_TOOLS_GAMEPAD_MONITOR` 后，
生成 `bin/openember_gamepad_monitor`。支持 `--device EVENT_PATH`、`--seconds N`；
省略时长持续运行到 Ctrl+C。退出码：0 曾连接成功，2 全程未连接，1 参数或运行异常。
原始监视工具和产品监视工具都不发送控制命令。

独立运行测试与工具，无需配置全部框架依赖：

```bash
cmake -S components/remote_control/test -B /tmp/openember-remote-tests \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Werror'
cmake --build /tmp/openember-remote-tests -j4
ctest --test-dir /tmp/openember-remote-tests --output-on-failure
/tmp/openember-remote-tests/openember_gamepad_monitor --seconds 5
```

14 项自动测试覆盖回中、边沿、deadman、死区/方向、采样期限、会话切换、配置校验，
以及真实后端在模拟 syscall 边界下的设备身份、事件包提交、同步丢失、溢出、读取失败和挂断。
这些测试不替代真实手柄按键与无线失联验证。
