# 通用整机状态与产品管理入口

本轮提供 `openember::system_state` 通用聚合组件与 system/v1 公共状态协议，并在四足产品实现独立的 `quadruped_system_manager`。这是整机管理的状态基础：当前只读聚合和发布，不申请控制权、不重启进程、不下发停机或关节命令。整机命令、授权门控和异步处置回执尚未接入，不能将当前聚合器当成已经生效的安全控制器。

## 职责

launch_manager 管理进程启动、就绪、存活和关闭，同时承载默认 Link router；alerts 保存结构化告警并记录日志；health_monitor 关注节点与资源健康；system_manager 聚合产品状态并在后续承载全局处置策略；motion 与 HI 保留各自控制状态机和本地保护。后续全局状态管理通过公共请求调用模块，不能绕过 motion 的唯一 owner。

通用组件在 components/system_state，不放入底层 core。它不依赖 Link、MuJoCo、运动协议或日志；由调用方传入经过校验的观测和接收方单调时间。产品负责机型配置、告警规则、业务状态映射与执行器。未来其他形态机器人可以复用 reducer 和公共协议，替换产品适配器。

## 三个独立维度

| 维度 | 状态 |
| --- | --- |
| 生命周期 | Initializing、Standby、Running、Suspended、Recovering、ShuttingDown |
| 健康 | Unknown、Normal、Warning、Degraded、Fault |
| 执行器 | Unknown、Disarmed、Armed、EmergencyStop |

例如运行中的机器人可同时处于 Warning；倒地告警可存在于攀爬或恢复过程中。正常健康不代表可运动，进程就绪不代表有控制权，告警消失不自动恢复运动授权。组件的 phase 与 actuation 由产品观测适配器提供；第一版服务从 motion 状态推导这些字段，尚未成为独立的目标状态机。

## 告警观测与规则

输入同时使用 `/alerts/events` 和 `/alerts/state`。完整快照按 publisher_instance 替换该发布者的全部条件，空列表会清除旧条件；事件只更新条件，不延长发布者的存活期限。重复和倒序 revision 不回放，事件 revision 有缺口时标为 Unknown，等待完整快照恢复。新实例必须先有快照，旧实例后续重放拒绝；每来源保留最多16个退出实例，超过后需重启管理器，不自动忘记旧实例身份。

必须在产品配置中列出期望告警发布节点。未收到快照、offline、心跳过期、条件 UNKNOWN 或快照不完整都不能推断正常。以接收方单调时钟判断新鲜度，不比较不同发布者的源采样时间。事件和快照在传输端有载荷限制；reducer 最多32来源、每来源256条件、256条规则。

规则按告警 source/code 精确匹配，响应分为 observe、degrade、fault。默认 WARNING 映射 Warning，ERROR 映射 Degraded；明确 fault 的规则映射 Fault。observe 即使针对 ERROR 也只映射 Warning。规则只改变健康结论，当前不执行控制动作；不按日志文本、单一倾角值或所有 ERROR 一律锁死。关键告警与未知来源同时存在时保留 Fault，并通过 unavailable_sources 表达证据缺失。

## 协议和后续工作

`openember-msgs` 的 system/v1/SystemState 提供 manager_session、序号、生命周期、健康、执行器状态、告警计数、缺失来源、online、stale_after_ms 和当前 motion_session。状态的权威性限定在当前 manager_session；订阅方需要自己的超时判断，最后一条正常状态不能永久有效。设备/namespace 的实际 topic 前缀由 Link 提供。

产品服务发布 `/system/state` 并提供只读 `/system/state/get`；后者返回最近快照，不提供状态转换命令或控制令牌。当前本机开发协议没有为公开状态增加认证，不能宣称是跨网络的可信安全控制面。

下一阶段增加经过授权的整机请求、转换代次、明确的目标/实际状态、幂等结果以及进入/退出动作；再接入按上下文选择的停机、降级、人工恢复策略和执行完成确认。控制动作失败必须保留未知/故障证据，不能在发出请求时提前宣布挂起或恢复完成。OS 只监督 launch_manager，不与它同时监督同一子进程。

## 验证记录

本轮宿主机验证通过通用 reducer 测试、依赖编排测试和真实 MuJoCo 编排集成测试。集成测试使用独立端口和临时 observe/control 凭据，覆盖初始化先后、运行中告警、事件缺口与快照恢复、发布者离线和旧实例回放，以及运动服务崩溃后下游重建且新会话仍未使能。原有 supervision 回归和六项 Link 断连/权限/边界/重启/撤权/官方窗口回归通过。未验证真机、开机启动和自动安全处置。
