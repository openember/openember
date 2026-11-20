# 产品自有 Hardware Interface 端点

HI 现在提供源码集成目标 `openember::hardware_interface`，原 `openember_hardware_interface`
可执行程序保留。不是二进制插件 ABI 或动态驱动加载系统；产品与框架使用相同提交和工具链重新编译。

## 组合方式

启用 `OPENEMBER_SERVICE_HARDWARE_INTERFACE`（依赖 Link、Messages、sensor、yaml-cpp）。
产品链接库、实现 `HardwareEndpoint`，在自己的可执行程序中显式注册，不修改框架的工厂分支：

```cpp
namespace hi = openember::services::hardware_interface;
hi::EndpointRegistry registry;
registry.Register("product_sensor", "product_driver",
    {"product.messages.Sample", "", openember::msgs::device::v1::DEVICE_CATEGORY_SENSOR},
    [validated_options](const hi::EndpointConfig& config, const hi::EndpointContext& context) {
        return std::make_unique<ProductEndpoint>(config, context, validated_options);
    });
hi::HardwareInterfaceApp app(node, config, app_options, std::move(registry));
if (!app.Start()) { /* 记录失败，退出；不能继续认为设备已就绪 */ }
app.Spin(should_stop);
app.Stop();
// app / endpoint workers 必须先停止，再 Shutdown transport。
```

自定义 SDK 参数由产品解析、校验并通过闭包注入；不把 SDK 类型或任意 YAML 对象塞进框架公共接口。
现有 `EndpointConfig` 继续承载 id/type/driver/mode/topic/频率等通用字段。

## 匹配与发现

- 精确匹配 `(type, driver)`；重复注册、空注册、未知驱动、空工厂结果、返回错误 id 均拒绝。
- 空注册表就是产品白名单。需使用内置端点时显式从 `BuiltinEndpointRegistry()` 开始添加。
  旧的三参数 App 构造函数仍使用内置注册表。
- 内置 Mock 驱动按编译选项注册 `mock_imu/mock_temperature/mock_gnss/mock_joint_controller`，只接受 `mode=mock`。启用核心可选 Yesense 驱动与 HI IMU 端点后，还注册 `imu/yesense_readonly`，支持通过 `device.yaml` 装配，见 [YIS321 配置说明](yesense-imu.md)。未知真实驱动不会回退成 Mock，内置端点编译开关仍有效。
- 描述符提供 sample/command Protobuf 类型名和设备类别，用于 NodeInfo 与 DeviceInfo。
  无 command 类型的端点禁止配置 command_topic。描述符不是授权或 schema 验证器，产品负责注册正确消息类型。
- 节点发现、心跳和诊断主题现在使用 `app_options.node_name`，默认名称不变；
  `app_options.process_name` 可设置产品进程名，避免被误标为框架默认可执行程序；
  device 主题仍使用 endpoint id，同一 robot_id 下需确保全局唯一。
- 当前描述符只列 sample/command topic，自定义 RPC 不自动加入 NodeInfo；客户端需按产品契约连接。

## 生命周期责任

- Factory 只构造对象，不应启动设备/线程。Configure 校验配置；Start 获取资源并启动线程。
- Stop 必须 `noexcept`、可重复、可在未配置/部分启动/失败后调用，需及时停止并 join 自有线程。
- App 在启动前验证包括程序构造配置的 id 唯一性、非空字段与有限正频率。
- critical 端点构造/配置/启动失败或启动异常，回收已构造的端点；未成功启动不可进入 Spin。
  非 critical 的不可用工厂会记录错误并跳过，不能将该缺失设备当作 ready；产品需把必需设备设为 critical。
- 同一 App 仅允许一次 Start 尝试，不支持失败后重试或停止后重启。重启需新建 App 与 Node。
  Node 持有通信回调，回调不得捕获生命周期更短的裸 `this`；推荐共享快照状态。
- 生命周期操作由单一所有者线程串行调用；端点自己的 Status 必须允许与工作线程并发读取。
- Spin 发现 critical 端点进入 Error 时停止所有端点并抛出异常，交给进程退出/监督策略处理。
  非致命数据 stale 用 timeout_count/诊断表达，不等同于进程或硬件停止。
  Spin 是约 1 Hz 的管理循环，不是电机实时 watchdog；真实硬件底线必须由端点/设备独立保证。
- 心跳、DeviceState 和诊断会反映 timeout/error；历史计数非零会保持 warning，不能据此单独决定运动准入。

## 验证

### 复合设备的附加只读主题

端点可覆盖 `AdditionalSampleTopics()`，返回 `{topic, type}` 列表。默认空列表，
保持现有单主题端点的源码行为；新增虚方法后需重新构建端点库，不保证旧二进制 ABI。
元数据在 Configure 之前可用，生命周期内不可改变，不能在该方法中打开硬件。
这些主题只会作为 Publisher 加入 NodeInfo，不声明命令订阅或授权控制。

注册工厂拒绝空类型、非绝对主题、端点内重复以及与主主题/命令主题冲突；
App 在任何端点启动前检查多个端点间的主/附加采样主题冲突。
框架只登记元数据，端点仍需自己 Advertise/Publish，不能仅声明而不实现。
产品 PMU 端点以此发布 PowerState 与两个 BatteryState，框架不认识电池数量或协议索引。

所有已构造端点均启动失败时，即使全为非 critical，也返回启动失败；
部分非 critical 端点失败仍允许其他成功端点运行。运行期非 critical Error 可继续发布诊断，
不等同于设备健康。新增冲突与全失败测试，完整框架回归 29/29 通过。

框架新增 9 个 `endpoint_*` 回归：注册匹配、重复、空/错误工厂、只读命令拒绝；生命周期与重复启动；
重复 id、NaN 频率、缺失驱动；Configure/Start 失败、异常与 critical 运行错误的清理。
既有内置硬件回归继续执行。

首个外部使用者为 openember-quadruped 的 `packages/quadruped_hardware`。产品只注册一个聚合的
`quadruped_mock/p05w_mock` 只读端点，不开放真实硬件或运动命令；它证明产品自有消息、线程和
故障诊断可以在产品仓维护，并非已经完成 IMU/PMU/关节真实 SDK 的公共接口适配。
