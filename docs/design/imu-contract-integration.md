# 公共 IMU 契约接入

消息基线：`openember-msgs.revision`。本批新增协议字段位于 openember-msgs `3cf1a0e`，
保留 ImuSample 的 1–9 号字段，追加姿态/有效性/参考坐标系/接收时钟域等字段。
本文件记录实现侧行为，完整 wire 规则见消息仓 `docs/imu-contract.md`。

## 公共 API

- `openember/sensor/imu.hpp`：C++ ImuSample 与公共协议同名语义；Quaternion 使用 w/x/y/z 命名分量，默认全零且无效。
- `openember/sensor/imu_validation.hpp`：`IsFreshOrientation` 检查姿态、来源元数据、单位范数和接收年龄；
  `IsFreshImu` 另外要求有效有限的加速度与角速度，只有声明有效时才检查可选磁场/温度。
- `ImuTracker`：单线程、单一 session、第一帧固定 source/frame/reference/mounting；后续序号和接收时间严格递增。
  首帧 sequence=0 合法；重复、倒退、旧会话、变化的元数据或无效样本不会替换/刷新最后有效样本。
  重启需要显式创建新 tracker，不自动信任新来源。首帧来源仍需产品按已知机器清单校验。
- `openember/services/hardware_interface/sensor_message_adapter.hpp`：公开 ToProto/FromProto，产品通过
  `openember::hardware_interface` 链接即可使用，不再包含框架内部目录。
  旧 `adapters/*.hpp` 包装头保留，避免破坏既有框架源码。

转换保留数值、原采样时间、原接收时间和序号，不归一化非法 quaternion，也不重新打采样时间戳。
FromProto 对缺失的 vector/orientation 子消息强制对应 valid=false，防止未携带数据被当作有效零值。
其他非法数值原样保留，由显式 validator 拒绝。转换不等于通过健康/运动准入。

## 时间与坐标边界

`receive_time_monotonic_ns` 必须与传入 now 属于同一个生产者时钟域。跨进程同主机比较也要明确确认
时钟来源；跨机器不能用本机 steady_clock 直接减远端值。远端消费者还需要本机接收超时、序号进展、
会话授权和旧消息重放处理。`sample_time_monotonic_ns` 的设备采样时钟不替代接收时钟。

R_reference_sensor 表示将 frame_id 中的向量旋转到 reference_frame_id。框架不默认为 ENU/NED，
不猜轴向、安装旋转或重力补偿语义，不生成姿态估计器。这些仍需 SDK 适配/机型标定验证。
本批的 Fresh 是样本级健康，不代表同步观测、滤波收敛、关节就绪或允许电机使能。

## Mock 与旧消息

框架原 MockImu 继续输出合成加速度、角速度、温度，并显式标记这些通道有效；没有磁场或姿态估计，
因此 magnetic_field_valid/orientation_valid 仍为 false。Stop→Start 生成新 clock_domain 并重置序号；
已运行时重复 Start 不切换会话。不能因为旧 Mock 在运行就认为已具备机器人姿态。

产品可以提供显式合成姿态的另一种 Mock，例如 quadruped 的静止 identity fixture。
那只是独立产品的测试输入，不改变框架旧 Mock 的含义。

## 构建与验证

配置阶段检查消息 target 的 `OPENEMBER_IMU_CONTRACT_VERSION>=1`，旧消息依赖会给出明确错误，
而不是在编译适配器时才遇到缺失成员。`openember-msgs.revision` 是已验证基线记录，**不会自动 checkout、
下载或锁定用户的 LOCAL 工作树**。新提交尚未推送时，用本机 LOCAL 消息源；不能假定远端 main 已包含它。

本机配置示例：

```bash
cmake -S . -B build-regression -C cmake/RegressionDefaults.cmake \
  -DOPENEMBER_MSGS_SOURCE=LOCAL \
  -DOPENEMBER_MSGS_LOCAL_SOURCE=/absolute/path/to/openember-msgs
cmake --build build-regression --parallel 4
ctest --test-dir build-regression --output-on-failure -j 4
```

四项 `imu_*` 测试覆盖：非对称 quaternion 和原时间转换、旧消息/缺失子消息；无效标志/NaN/Inf/
过期与元数据；重复帧/倒退/新会话；旧 Mock 的有效性和重启行为。
2026-11-03：框架完整构建及 21/21 CTest 通过；消息仓 C++ 兼容与 Nanopb C 往返 2/2 通过。
