# Rover Demo（巡游小车）设计草案

- 状态：设计草案（代码尚未落地）
- 更新日期：2026-10-28
- 相关：
  - 三档总览：[demos-overview.md](./demos-overview.md)
  - 应用层：[Application Layer 设计指导](../architecture/application-layer-design-guide.md)
  - 运行时：[Runtime Supervision 与 Product App](../architecture/runtime-supervision-and-product-app-design.md)
  - Hardware Interface：[hardware-interface.md](./hardware-interface.md)
  - 用户文档（规划中）：openember-docs `docs/rover-demo.md`
  - 对照入门示例：`apps/turtle_demo`、openember-docs [Turtle Demo](https://github.com/openember/openember-docs)

## 1. 目标

在 OpenEmber 中提供第二档**完整产品形态 Demo**：可在仿真中跑通，也可在 **¥3000 以内** DIY 真机上部署同一套软件包故事。

三档对照见 [demos-overview.md](./demos-overview.md)。命名不用泛化的 `robot_demo`，统一为 **`rover_demo`（巡游小车）**。

### 1.1 非目标

- 不把 Microduck / 双足 / 人形作为本 Demo 机体（许可 NC、成本与 RL 门槛不适合官方再分发 Demo）。
- 第一版不做完整 SLAM / 自主导航发行栈；激光用于展示测距 Topic 与简易避障即可。
- 不在三个 Demo 间复制 `system/` / `services/`；差异只在 App、launch 与驱动 package。

### 1.2 成功标准（MVP）

1. 宿主机：差速运动学仿真（或 Gazebo 可选）+ `cmd_vel` / odom 闭环，launch 可一键起。
2. 真机：同一 Product App 话题契约，更换 Hardware Interface 驱动后可遥控行走与急停。
3. 共用：`launch_manager`、health / config / device、可选 `web_console` / `host_monitor`。
4. 单机 BOM 目标价 **≤ ¥3000**（含激光或深度相机之一）。

## 2. 分层与打包

```text
apps/rover_demo/              # Product App：行为与安全策略
configs/rover.launch.yaml     # 本产品编排
services/hardware_interface/  # 传感 / 执行接入（Mock → 真机 package）
system/*                      # 监督与运维（按 launch 裁剪）
services/web_console 等       # 可选运维面
```

同一工程通过 **Kconfig / defconfig + launch** 打出不同软件包：`turtle` / `rover` / `iot` 选不同 App 与节点子集，中间件不按产品分叉。

## 3. 话题与消息（草案）

与 Turtle 对齐平面速度，并扩展里程计与测距。具体 Protobuf 包名以 `openember-msgs` 落地时为准；可先复用 `geometry/v1`，不足再增 `nav` / `sensor` 包。

| Topic（建议） | 类型方向 | 说明 |
|---------------|----------|------|
| `/rover/cmd_vel` | Twist2D ← teleop / App | 线速度 + 角速度 |
| `/rover/odom` 或 `/rover/pose` | Pose2D / Odometry → | 积分或编码器融合位姿 |
| `/rover/scan`（可选） | LaserScan 类 → | 2D 激光；无硬件时 Mock |
| `/rover/estop` 或等价服务 | 安全输入 | 急停；不可被普通 cmd 覆盖 |
| 系统话题 | heartbeat / health 等 | 与现有 system 节点一致 |

安全约定（对齐优化计划）：

- 失联超时 → 本地停车（不等待远端 health 汇总）。
- 急停 / 禁用优先于普通速度指令。
- 执行器单写者：仅 HI / 约定执行路径写电机。

## 4. 软件分期

| 阶段 | 交付 | 验收 |
|------|------|------|
| A. 骨架 | `OPENEMBER_APP_ROVER_DEMO`、空 App、`rover.launch.yaml`、文档入口 | 可构建、可 launch 起 App + 共用 system |
| B. 宿主机仿真 | 软件差速积分或接 Gazebo；teleop；odom | 无真机可演示行走与急停 |
| C. Mock HI | 经 Hardware Interface Mock 发假 scan / 收 cmd | 换驱动不改 App 话题 |
| D. 真机驱动 | 电机 + 编码器/IMU；可选激光 | ¥3000 BOM 实车上遥控与停车 |
| E. 运维面 | web_console 状态、host 指标 | 浏览器可见速度 / 连接状态 |

## 5. 真机 BOM（¥3000 舒适区）

价格随渠道波动，下表为 **量级**，采购前按发票复核；第一版优先「能稳定跑 Demo」，不追求最低价。

| 类别 | 建议选型方向 | 量级（¥） |
|------|----------------|-----------|
| 算力 | 树莓派 4/5、Radxa、香橙派等同档 SBC + 卡 | 200–500 |
| 底盘 | 差速两驱或四驱套件（含电机、驱动、结构） | 150–400 |
| 电机与驱动 | 若套件不足：编码器电机 + TB6612/同类 | 100–250 |
| 姿态 | 廉价 6/9 轴 IMU | 30–80 |
| 测距（择一） | **2D 激光**（RPLIDAR A1 / 国产同类）或深度相机 | 400–1200 |
| 电源 | 12V/5V 电池组、BEC、线材开关 | 100–250 |
| 结构与打印 | 支架、减震、线槽、打印件 | 50–200 |
| 遥控 / 调试 | 手柄或依赖 SSH + teleop 节点 | 0–150 |
| **合计目标** | 含激光的完整 Demo 车 | **≤ 3000** |

### 5.1 配置建议

- **推荐默认：** 差速底盘 + 编码器 + IMU + **2D 激光** + SBC。  
- **降本选项：** 去掉激光，改超声波阵列（仍建议保留至少一路测距，否则与 Turtle 差异过弱）。  
- **升配（仍尽量压在 3000 内）：** 更好底盘轴承/轮胎、独立轮速板；深度相机与激光不同时上，避免超支与集成爆炸。

### 5.2 仿真

- **默认路径：** 宿主机进程内差速运动学（延续 turtle 经验，依赖少）。  
- **可选路径：** Gazebo / Ignition + 自有或 Apache/BSD 许可的差速 URDF；不引入 NC 商业外观资源。  
- 仿真与真机共用 Topic；差异隔离在 HI / 仿真适配层。

## 6. 与其它 Demo / 参考项目的关系

| 对象 | 关系 |
|------|------|
| `turtle_demo` | 通信与平面速度的最小集；Rover 是其「有底盘与传感」的升级，不是替换 |
| `smart_device_demo` | IoT / 数采故事；Rover 偏运动与安全，不合并成一个 App |
| Microduck 生态 | 仅作 OTA / 监督 / 单写者等**工程纪律参考**；不作为本 Demo 机体或可再分发网格来源 |
| ROS TurtleBot 等 | 可借鉴开源差速 URDF / 教学结构；引入前核对许可证，避免把整栈 ROS 依赖绑进默认构建 |

## 7. 文档与仓库约定

| 位置 | 内容 |
|------|------|
| 本文（`openember/docs/design/rover-demo.md`） | 设计、BOM、分期、非目标 |
| `openember-docs/docs/rover-demo.md` | 用户向说明（规划状态 → 落地后补运行步骤） |
| `apps/rover_demo/` | 实现落地后与 Kconfig / CMake 同步 |
| `configs/rover.launch.yaml` | 产品 launch |

变更 BOM 上限、话题名或 MVP 范围时，同步改本文与 openember-docs，避免两处叙事漂移。
