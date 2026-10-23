# MicroDuck 对 OpenEmber 的启发与可借鉴点

- 状态：优化指导，待分阶段落地
- 更新日期：2026-09-16
- 阅读基线：MicroDuck `590b986`；OpenEmber `4225624`
- 相关文档：
  - [分层架构](layered-architecture-design-guide.md)
  - [Runtime Supervision 与 Product App](runtime-supervision-and-product-app-design.md)
  - [Hardware Interface](../design/hardware-interface.md)
  - openember-docs：`docs/rbu-specification.md`（RBU Specification）
  - 参考仓库：OpenEmber 工作区中的 `microduck`（Pollen Robotics 机器鸭产品固件）

本文记录对 MicroDuck 代码与设计文档的对照阅读结论：哪些思想值得 OpenEmber 吸收，哪些属于产品特化不应搬进 middleware。

本文中的“当前”指上述本地代码基线；“建议”和验收项是后续目标，不表示已经实现。本次核对采用源码、部署脚本和测试代码阅读，未执行 MicroDuck 测试或真机验证。具体机制落地时，应同步更新对应的权威设计文档和本文的进度记录。

---

## 一、结论先行

**有启发，价值高——但不是照搬架构，而是学习产品级工程纪律与故障隔离。**

| | MicroDuck | OpenEmber |
| --- | --- | --- |
| 定位 | 单机产品固件 | 通用机器人 / 智能设备 middleware runtime |
| 形态 | 固定约 7 个 systemd daemon | Node/Link + `system/` / `services/` / `apps/` |
| 通信 | 本机 Unix socket + JSON-RPC NDJSON，刻意无 broker | Link / Zenoh + `openember-msgs` |
| 监督 | systemd | `launch_manager` |
| OTA | 应用目录 + 原子 symlink + health gate + golden | RBU 多组件 Bundle 规范；`ota_agent` 仍为占位程序 |
| 硬件 | 电机与同总线 IMU 归 robotd，ToF / 媒体另有进程 | sensor / actuator 领域接口与 `hardware_interface` Mock 闭环；真实 package 替换待实现 |

OpenEmber **不应**改成 MicroDuck 那种固定 daemon 拓扑；应把「恢复路径独立、意图权威、OTA 自救细节」吸收进现有分层与 RBU Agent 设计。

最值得直接落地的三条：

1. 配置、更新、运维接入不依赖业务控制进程，并具备进程外恢复入口。
2. Intent → Safety 单写者，禁止旁路写执行器。
3. RBU Agent 级：原子激活、health gate、golden、进程外 boot rescue，并按测试层级验证。

---

## 二、MicroDuck 是什么

MicroDuck 是约 25 cm / 800 g 双足宠物机器人的板端固件仓：Rockchip RK3566（Radxa Zero 3W），以 **Rust Cargo workspace + systemd** 为主，也包含 C 驱动和部署脚本。

- `robotd`：50 Hz RL / ONNX 策略环，驱动舵机与总线 IMU。
- `configd` / `updaterd` / `btd`：配网、签名 OTA、BLE——构成**控制环挂掉后仍可达**的恢复路径。
- `padd` / `mediad` / `tofd`：手柄、WebRTC 媒体、ToF 等传输与感知。

主通信路径使用 Unix socket / JSON-RPC。更完整的自主行为方案仍在 `docs/ideas/`，应与已经实现的控制逻辑区分。

关键设计文档（microduck 仓内）：

| 文档 | 拥有的机制 |
| --- | --- |
| `docs/design/architecture.md` | 服务拆分、IPC、状态所有权、安全与权威 |
| `docs/design/robotd-design.md` | 控制环、总线所有权、safety |
| `docs/design/updater-design.md` | 验签、原子 swap、health gate、回滚 |
| `docs/design/restart-order.md` | 谁在何时重启 |
| `docs/design/boot-recovery-net.md` | golden / 进程外救援网 |

MicroDuck 文档纪律值得单独记下：**一机制一文档所有者**，其它页只链接，避免 OTA/重启叙事多处漂移（见其 `docs/README.md`）。

---

## 三、架构对照（文本）

### 3.1 MicroDuck 形状

```text
gamepad / phone / laptop / WebRTC / GitHub Releases
        │
        ▼
  Unix socket · JSON-RPC · SO_PEERCRED
        │
   ┌────┼────────────┬────────────┐
   ▼    ▼            ▼            ▼
robotd  configd   updaterd     mediad/padd/tofd
(50Hz)  wifi/name  verify/swap  transports / sensors
safety  pad bond   health gate
   │                  │
Dynamixel          current → releases/<ver>/
```

要点：`robotd` 拥有电机总线和同总线 IMU；相机、ToF 等由其他服务处理。`configd` / `updaterd` / `btd` **无对 robotd 的硬依赖**。

### 3.2 OpenEmber 形状（现行）

```text
apps / system / services / examples / tools
        ↓
communication / core（Node · Link · pub/sub）
        ↓
components（sensor · actuator · transport · logging …）
        ↓
platform（osal · lpio）
```

已落地：`launch_manager`、Link 节点通信、`hardware_interface` mock 闭环等。
待完善：`core/include/openember/executor.hpp` 和 `core/src/executor.cpp` 为空文件；订阅回调由 transport 直接调用；`ota_agent` 创建 Node 后等待约 200 ms 即退出。统一执行模型、完整 OTA 和产品级安全策略均需继续建设。

`communication/` 当前主要承载 Kconfig 菜单；Link 的节点 API 位于 `core/`，Zenoh 后端位于 `components/transport/`，不能把菜单层次等同于源码目录实现。

职责边界（既有设计）：

```text
launch_manager 管 runtime 是否活着；
health_monitor 管是否健康；
device_manager 管设备是否可用；
config_service 管配置；
product app 管产品该做什么。
```

---

## 四、产品逻辑 vs 可复用思想

| MicroDuck 特有（留在产品） | 可抽象给 OpenEmber |
| --- | --- |
| 鸭外形、ONNX gait、叫声 / chorale、手柄映射 | 恢复三件套与控制进程解耦 |
| Dynamixel 单总线 + 同总线 IMU | 「单写者总线 / 环外感知」拆分规则 |
| BLE GATT 配网与更新触发 | 「一 API、多薄适配器」产品 SDK 形状 |
| GitHub Releases + minisign 具体源 | 验签、原子激活、health gate、golden、进程外救援 |
| 宠物 FSM / 情绪模型 | Intent 层 + 权威仲裁 + deadman（行为实现可换） |

---

## 五、可借鉴点（按价值）

### 5.1 High — 建议纳入设计与实现清单

#### 1. 恢复路径不得依赖控制进程

依据：MicroDuck `architecture.md` —— `btd` / `configd` / `updaterd` 无对 `robotd` 的 systemd / 硬 IPC 依赖。

对 OpenEmber：

- `ota_agent`、配网 / 配置、launch 控制面应与业务 Node、`hardware_interface` 故障隔离。
- 业务控制环崩溃时，仍应能更新、回滚、改配置。

#### 2. Intent → 权威执行，禁止旁路写执行器

依据：客户端只发意图；`safety` 独占 `RobotIo`，只有 safety 能写电机。

对 OpenEmber：

- Product App 的行为意图与 `JointCommand` 关节目标保留不同层次：运动控制逻辑把意图转换为关节目标，Endpoint 负责接入与执行边界，不承担产品行为。
- 安全校验与真实写口必须唯一；具体所有权与权限要求见第七节。

#### 3. OTA：原子激活 + health gate + golden / 进程外救援

依据：`updater-design.md`、`boot-recovery-net.md` —— 验签 → 原子 swap `current` → 健康门控 → 失败回滚；boot 救援装在进程外路径，不经 `current`。

对 OpenEmber：

- RBU 已定义 Bundle、`reboot_after` 等**契约**；MicroDuck 补的是 **Agent 执行与自救细节**。
- `ota_agent` / RBU 实现应对齐：激活后探测、失败回滚、updater 自身损坏时仍可救援。

#### 4. 控制环永不同步等待其它服务

依据：intent last-value-wins；环只读缓存快照；`MissedTickBehavior::Skip`。

对 OpenEmber：

- 近实时 / 控制节点：订阅进入本地缓存；环内不阻塞 RPC、不因感知失败引入抖动。

#### 5. 健康门控只归因「软件可责备」

依据：电池 / 温度等板态不进入 rollback 判定；与软件 unhealthy 区分。

对 OpenEmber：

- `health_monitor` / RBU 验收：同时记录严重程度与故障来源。软件异常只是回滚候选，仍需结合新版本试运行窗口与健康策略；电源 / 热等问题通常需要产品降级，不应直接触发回滚。

#### 6. 恢复路径 IPC 契约保持轻量

依据：`duck-ipc-proto` 仅轻依赖；更新 / 恢复工具不链接重型 ML / 媒体栈。

对 OpenEmber：

- OTA / launch 控制面避免绑定过重编解码或业务库；`openember-msgs` 核心消息保持克制。

### 5.2 Medium — 工程与边界

| # | 启示 | 对 OpenEmber |
| --- | --- | --- |
| 7 | 本机控制面：UDS + 文件系统权限 + peer cred | 本机管理 API 可参考；远程另走鉴权通道 |
| 8 | Features not frames；重/慢感知出控制进程 | 与 sensor endpoint 拆分一致：环内消费特征，帧留在感知侧 |
| 9 | 更新重启集编码：永不重启者 / 回复后再重启者 | OTA apply 中途勿杀 updater；规则进设计而非散落配置 |
| 10 | Orphan unit：降级后 unit 指向不存在二进制则拒绝安装 | 多组件 RBU 安装 / 回滚前检查可执行入口可达 |
| 11 | 一机制一文档所有者 | `openember/docs` 与 openember-docs 指定权威页，避免叙事分叉 |
| 12 | 控制核纯库，服务层才绑平台 | `components/` vs `services/`：HAL / 控制逻辑尽量无异步运行时硬绑定 |

### 5.3 Low — 了解即可

- API version 报告但不全局握手拒绝对话；未知 params 严格拒绝。
- installed vs running 双轨版本（updater 延迟重启窗口）。
- 多传输同一调用面（BLE / UDS / WebRTC）作产品 SDK，不塞进 core。
- 宠物 FSM / 社交能力：只抽象「行为层插件 + 权威仲裁」，实现放 `apps/`。

---

## 六、明确不建议照搬的部分

1. **用固定 daemon 集替代 Node/Link 图** —— 与 OpenEmber 通用 runtime 目标冲突。
2. **用 JSON-RPC/UDS 替代 Link/Zenoh** —— 本机管理面可参考，整机通信面不必替换。
3. **把 ONNX gait / 宠物 FSM 放进 core** —— 属 product app / 可选组件。
4. **整仓 Rust + 仅 systemd 监督** —— OpenEmber 已选 C++ / Kconfig / `launch_manager` 路径；可共存于产品镜像，但不是框架默认模型。
5. **仅应用目录 OTA、不覆盖 OS** —— MicroDuck 范围更窄；OpenEmber RBU 有意覆盖 Linux Image、应用包与 MCU 固件等多组件，执行层可借其自救细节，范围不必收窄。

---

## 七、实现边界与优化指导

### 7.1 执行模型与控制周期

MicroDuck 的参考入口是 `robotd/src/main.rs`、`robotd/src/intents.rs` 和 `robotd/src/control.rs`：周期使用 `MissedTickBehavior::Skip`，控制计算消费意图快照。其 50 Hz 循环仍是普通 Linux 调度下的任务，不能据此承诺硬实时。

OpenEmber 当前 `core/include/openember/node.hpp` 与 `components/transport/src/subscriber.cpp` 直接在 transport 回调路径反序列化并调用用户函数；`JointControllerEndpoint::HandleCommand()` 直接调用 `SetCommand()`。Mock 内部有锁和命令缓存，但未来替换真实 package 时，需要明确 `SetCommand()` 是否只提交数据、是否可能执行 I/O。

建议先在 Joint Endpoint 落地一个简单模型，再依据复用需求完善 `core::Executor`：

```text
Link 回调：解析 / 基本校验 -> 更新本地命令快照
固定周期：取快照 -> 检查权限与时效 -> 控制 / 安全校验 -> 唯一硬件写口
状态发布：读取状态快照 -> 序列化 / Link 发布
```

- 连续位置、速度目标采用最新值覆盖；启停、急停、清故障等离散操作必须定义顺序和确认方式，不能一起被覆盖丢失。
- 控制周期不等待同步 RPC、网络发送或慢速日志；必要的总线 I/O 应有明确超时。高带宽感知在其他线程或进程中处理。
- 使用单调时钟安排周期，错过周期后跳过过期任务；记录周期耗时、超期次数和样本年龄。
- deadman（失联超时保护）使用接收端本地单调时钟计算命令年龄。跨设备发送者的单调时间不能直接与本机比较；发送时间与序号另用于延迟分析、乱序和重放处理。
- Executor 第一版只需明确回调线程、有限队列、停止及回调释放规则。实际控制频率与最坏延迟由产品测量；硬实时需求另行评估专用线程、实时 OS 或 MCU。

### 7.2 单写者与 Safety 所有权

MicroDuck 的 `duck-control/src/safety.rs` 中，`Safety<T: RobotIo>` 私有持有 I/O 对象，`robotd/src/control.rs` 只计算目标。可借鉴的是让接口和对象所有权落实安全边界。

OpenEmber 建议：

- 真实 Hardware Package 的总线写资源由一个不可复制的控制对象持有，外部只看到提交目标与读取状态的接口。
- 限位、非有限数值拒绝、速度 / 力矩上限、命令有效期等检查覆盖所有写路径，包括调试和维护入口。
- 控制源初期可固定为一个产品节点；出现遥控、自主控制等多源需求时，再增加明确的控制权与优先级规则。`source_node` 字符串只是一项消息数据，不能单独作为可信身份凭证。
- 对象独占解决进程内旁路；进程间还需设备访问权限及总线占用约束。move-only 对象无法阻止其他进程再次打开同一设备。
- 安全输出由硬件和产品共同定义：轮式底盘停速、双足保持平衡、机械臂保持或制动可能各不相同。当前 Mock 的 disable / effort 归零不能自动成为所有产品的安全策略。
- 进程崩溃时析构函数无法保证执行。真实执行器应有独立看门狗或控制板超时保护，并测试进程失联后的实际行为。

第一版优先在已有 `IJointController` 与 Endpoint 边界内落实这些约束；有第二个实现需要共享时再提取公共 Safety 组件。

### 7.3 按总线和故障域部署 Hardware Interface

统一领域接口可以支持多种进程布局。MicroDuck 将电机与同总线 IMU 放在一起，将 ToF、媒体放在其他进程，这为 OpenEmber 的部署边界提供了参考。

建议保留简单设备的单实例默认方式，产品需要隔离时再采用以下布局：

```text
hardware_interface.motion   JointController + 同总线反馈
hardware_interface.sensors  独立 IMU / Temperature / GNSS
camera_service             图像采集与媒体处理
```

这些名称是建议的逻辑实例名，并非当前新增的目录或可执行文件。共享物理总线的对象由同一 package 协调访问，不能因消息类型不同而各自打开总线；对独立慢设备则可按故障域拆分。

当前 `hardware_interface_app.cpp` 仍固定发布 `/nodes/hardware_interface/*` 和 `/diagnostics/hardware_interface`，设备 Topic 使用 endpoint ID。因此，多实例尚未完成：需要让节点 Topic 跟随节点身份、约束 runtime 内设备 ID 唯一、明确重启后的 instance 身份，并验证 device_manager / health_monitor 的聚合结果不会互相覆盖。

### 7.4 两级监督与恢复入口

建议产品镜像采用以下职责关系：

```text
操作系统级监督（systemd / init / watchdog）
  -> launch_manager（默认 Link router + OpenEmber 进程监督）
      -> system / services / Product App
  -> 独立恢复入口（可在 OpenEmber runtime 无法启动时执行）
```

- OS 级监督负责恢复 launch_manager；launch_manager 负责其管理的 OpenEmber 子进程。同一子进程只选一个直接监督者，避免两边同时重启。
- 配置与更新恢复路径应能承受 Product App 或硬件进程失效。若 Link router 随 launch_manager 一同失效，至少保留 OS 级重启与本地回滚入口。
- 救援工具、已知可用版本指针与恢复状态不得全部位于待更新的 release 内；ota_agent 自身损坏时也需要可执行的恢复路径。
- OTA 过程中先明确哪些进程必须继续运行、哪些在回复完成后重启；不要依赖被终止进程创建的子进程继续完成其自恢复。

这一阶段优先补产品部署单元和故障测试，不要求 launch_manager 内再实现一个通用服务管理器。

### 7.5 健康归因与 OTA 决策

健康严重程度与故障原因是两个维度。以下是建议的诊断分类，不是现有 Protobuf 枚举：

| 原因 | 例子 | 主要决策方 |
| --- | --- | --- |
| 软件异常 | 新程序无法加载、试运行反复崩溃 | 监督器恢复；ota_agent 按试运行策略判断回滚 |
| 硬件降级 | 低电量、过热、可选传感器缺失 | Product App 决定限速、暂停或降级 |
| 硬件故障 | 执行器故障、关键反馈丢失 | 本地安全执行路径立即保护，Product App 处理后续行为 |
| 通信陈旧 | 心跳或命令超时 | 本地超时保护；监督器按策略恢复连接或进程 |

`health_monitor` 聚合原因、严重程度、时间与来源；故障事实本身不等同于回滚指令。严重故障不能等待 health_monitor 的秒级汇总后才触发执行器保护。

MicroDuck 将“缺硬件时等待而非退出”作为部分启动救援监测对象的约束。OpenEmber 当前允许 critical Endpoint 启动失败退出，因此不能照搬“进程失败就证明新软件有问题”的推断，需要明确退出原因或单独的升级健康探测。

### 7.6 OTA 执行与持久化

`updater/src/store.rs`、`engine.rs`、`journal.rs`、`reconcile.rs` 及 `scripts/robot-rescue` 可作为执行机制参考。建议后续 `ota_agent` 设计覆盖：

1. 验签、目标设备与兼容性检查、staging 和安装前自检。
2. 更新事务单次执行约束、持久化阶段记录、启动后续跑或回滚。
3. 原子激活与明确的落盘顺序。原子 rename 解决切换可见性，断电持久性仍需文件和目录同步及实际存储验证。
4. 健康门控：同时观察安装版本和实际运行版本，按软件故障归因决定是否接受新版本。
5. 区分 previous（前一版本）和 golden（长期保留的已知可用版本），保留进程外救援及防止循环回滚的记录。
6. 重启集合与顺序、updater 自身延迟重启、降级后服务入口仍存在等检查。

这些动作应遵循 RBU 的组件能力：Linux 应用、系统镜像与 MCU 固件未必使用同一种激活或回滚方式。

### 7.7 验证应覆盖实际部署路径

MicroDuck 当前已有以下不同层级的测试入口：

| 入口 | 覆盖范围与限制 |
| --- | --- |
| `duck-control/src/io.rs` 中 FakeIo 及相关测试 | 控制与 I/O 契约；不能代表真实执行器行为 |
| `updater/tests/`、`xtask/tests/rescue.rs` | 更新决策、签名产物、恢复脚本等测试；部分系统调用由 stub 代替 |
| `scripts/board-test.sh` | ARM64 构建产物及容器中的安装路径验证；不等同于实物硬件测试 |
| `scripts/systemd-test.sh` | 按需运行的真实 systemd 容器测试，覆盖重启、延迟重启与安装失败；脚本明确未接入常规 CI |

测试入口已经存在，不能再概括为“没有 systemd 集成测试”；但也不能据此宣称 boot timer、真实断电与硬件故障全部验证完成。本次阅读未执行这些测试。

OpenEmber 初期采用现有 Mock 与测试工具即可：为每个真实 package 复用领域契约测试，覆盖启动 / 停止、超时、安全输出和无效命令；恢复机制另外使用临时安装树、真实进程与产品部署环境验证。

### 7.8 文档所有权

“当前实现”“建议设计”“现场验证”分别标注状态，一项机制指定一份权威文档，其余文档链接引用。当前分层文档仍含已删除的 `platform/hal` 结构示例，后续应明确为未来规划或同步到 `platform/lpio` 的实际边界。

本文负责记录取舍与优化顺序；Hardware Interface、Runtime Supervision、Link 和 OTA 的最终接口与行为分别回写各自的设计文档。RBU 格式契约继续以 openember-docs 中的规范为准。

## 八、推进顺序与验收

具体推进顺序、阶段状态与验收记录统一维护在 [OpenEmber 优化实施计划](openember-optimization-plan.md)。当前顺序为：回归基线、控制与安全、真实接入与多实例、运行与恢复、最小 OTA、按需提炼公共能力。下面保留按技术主题划分的验收要求，不再表示执行先后顺序。

| 技术主题 | 建议落点 | 最小验收条件 |
| --- | --- | --- |
| 控制执行模型 | Joint Endpoint；公共 Executor 按需提炼 | 回调只提交数据；突发消息不造成无界积压；急停不被普通目标覆盖；控制周期超期可观测；命令超时使用本地接收时间 |
| 单写者安全边界 | `components/actuator`、真实 Hardware Package | 所有写路径经过安全校验；NaN / Inf、越界与失联测试通过；进程崩溃后的保护另有验证 |
| 两级监督 | 产品部署配置、launch_manager 指南 | 杀死 Product App 后仍可运维；杀死 launch_manager 后能由 OS 恢复；无两个监督者争抢同一进程 |
| 健康归因与 OTA | health_monitor、ota_agent、RBU 执行设计 | 软件升级失败与低电量 / 缺硬件可区分；失败升级可回滚；恢复状态落盘；ota_agent 无法启动时仍可救援 |
| 多实例与真实接入 | Hardware Interface 配置、身份及 package 注入 | 两实例无 Topic / 设备 ID 冲突；停掉 sensors 不影响独立 motion；Mock 与真实 package 使用一致上层契约 |

每阶段的设计完成、代码完成和测试通过分别记录。表中的验收条件目前均不表示已整体通过。

建议按需新增 `docs/design/ota-agent-execution.md`，集中说明激活、探测、回滚和自救；控制周期与 Safety 边界先补入 Hardware Interface 设计，两级监督补入 Runtime Supervision 设计。

---

## 九、与 RBU 的关系

RBU（Robot Bundle Update）定义的是**整机 Release 契约与 Bundle 格式**（组件清单、完整性、`reboot_after` 等）。

MicroDuck updater 定义的是**窄范围应用包在单机上的执行与自救**。

二者互补：

```text
RBU Manifest / Bundle     →  带什么、目标态是什么、何处 reboot_after
OpenEmber ota_agent       →  如何验签/安装/激活/探测/回滚/续跑（可学 MicroDuck）
MicroDuck 产品细节        →  symlink 布局、minisign、GitHub 源等可替换实现
```

OpenEmber 应保持 RBU 的多组件广度，同时吸收 MicroDuck 在**健康门控与恢复路径**上的深度。

---

## 十、摘要

```text
学：故障隔离 · Intent/Safety 单写者 · OTA 激活与自救 · 环内不阻塞 · 健康归因 · 文档所有权
不学：固定 daemon 拓扑 · 替换 Link · 产品 FSM 进 core · 收窄成仅应用目录 OTA
落点：ota_agent / hardware_interface / launch 依赖规则 / health 语义 / docs 纪律
```

MicroDuck 证明：小产品固件也可以在故障隔离和 OTA 自救上做得很扎实。OpenEmber 应把这些变成 **system / services 的契约与实现清单**，而不是改写 middleware 形态。
