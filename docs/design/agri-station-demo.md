# Agri Station Demo（智慧农业监测站）设计草案

- 状态：宿主机 Mock MVP 已落地（`apps/agri_station_demo`，`configs/agri_station.launch.yaml`）；`smart_device_demo` 仍保留；真机传感驱动待后续
- 更新日期：2026-10-28
- 相关：
  - 三档总览：[demos-overview.md](./demos-overview.md)
  - [Runtime Supervision 与 Product App](../architecture/runtime-supervision-and-product-app-design.md)
  - [Hardware Interface](./hardware-interface.md)
  - [Rover Demo](./rover-demo.md)、`apps/turtle_demo`
  - 用户文档（规划中）：openember-docs `docs/agri-station-demo.md`
- 更名说明：原「Env Station / 室内环境监测」叙事易被理解成可穿戴或过轻的消费电子；已改为**智慧农业边缘监测站**（见 §1）。

## 1. 产品定位：为什么不是「智能手表」，而是智慧农业

OpenEmber 跑在 **Linux** 上，带着多进程 Link、launch 监督、config / health / web 运维面——体量对应的是**机柜/棚内/田边的边缘站**，不是手表、手环一类 MCU/RTOS 产品。智能手表极少跑通用 Linux + 此类框架；若第三档 Demo 看起来像「袖珍个人物联」，会让人觉得栈过重。

| 候选 | 与 Linux + OpenEmber 匹配度 | 结论 |
|------|------------------------------|------|
| 智能手表 / 手环 | 低（MCU、电池、封闭 OS） | 不做 |
| 室内空气盒子 | 中（能跑 Linux，但故事偏消费小家电） | 放弃默认名额 |
| **温室 / 田边农业监测站** | 高（常驻供电或太阳能、多传感、要告警与运维、可挂网关） | **选定** |

**选定：智慧农业监测站（Agri Station）**  
目录与包名：**`agri_station_demo`**。  
一句话：棚内或田间的 **Linux 边缘站**——空气温湿、光照、土壤墒情（可选 CO₂）→ 本地阈值与告警 → Web 运维；**不做农机底盘运动**（与 Rover 分工）。

现有 `smart_device_demo` 作骨架，文档先行用新名。

## 2. 为何能发挥 OpenEmber 架构特点

```text
launch_manager (router + 监督)
  ├── agri_station_app         # Product App：灌溉/通风相关阈值策略与告警（逻辑层，第一版可不驱实泵）
  ├── hardware_interface(*)    # 气候 / 光照 / 土壤等 Mock 或真机 package
  ├── health_monitor
  ├── config_service           # 采样周期、湿度下限、光照告警等
  ├── device_manager
  ├── web_console
  └── host_monitor（可选）
```

| 架构要点 | 在本 Demo 中的体现 |
|----------|-------------------|
| 产品档位匹配 | 边缘站常驻运行，多节点监督与 OTA/运维叙事成立 |
| App ≠ 监督 | App 决定「土壤过干告警 / 传感降级」；不起停其它进程 |
| health 事实 vs App 行为 | 土壤传感掉线 → degraded；App 可关闭墒情告警、保留空气监测 |
| config 调参 | 改土壤湿度阈值、采样周期，不重编 |
| HI 可替换 | Mock → I2C/ADC 真传感，话题不变 |
| 弱实时监测 | 相对 Rover 的运动急停；本站强调可观测、可配置、可降级 |

第一版 **只监测 + 告警**（可模拟「建议灌溉」事件）；实驱水泵/风机留作后续，避免 Demo 变成灌控项目。

## 3. 产品故事（用户可感知）

1. launch 拉起站端；web_console 显示棚内温湿、光照、土壤湿度（Mock 或真值）。  
2. 调低土壤湿度阈值 → 出现告警 / 状态变化。  
3. 停掉土壤传感进程 → health 降级，空气与光照仍更新。  
4. （可选）外发告警摘要；第一版以站内 Link + Web 为主。

## 4. 话题与消息（草案）

前缀 `/agri/...`；类型在 `openember-msgs` 落地。

| Topic（建议） | 方向 | 说明 |
|---------------|------|------|
| `/agri/air/temperature` | HI → | ℃（AHT20） |
| `/agri/air/humidity` | HI → | %RH |
| `/agri/light/illuminance` | HI → | lux（BH1750） |
| `/agri/soil/moisture` | HI → | 相对湿度或原始比例（电容探针 + ADC） |
| `/agri/air/co2`（可选） | HI → | ppm（SCD40，温室补气故事） |
| `/agri/station/status` | App → | booting / running / degraded / fault |
| `/agri/station/alert` | App → | 过干、过湿、低温、弱光等 |
| 参数 get/set | ↔ config | 周期与阈值 |
| `/host/resources` | host_monitor → | 复用 |

## 5. 软件分期

| 阶段 | 交付 | 验收 |
|------|------|------|
| A. 命名与文档 | 本文 + 用户文档 | 叙事为智慧农业监测站 |
| B. App 行为 | `agri_station_app`：订传感、状态/告警 | Mock 过干可观测告警 |
| C. 多路传感 | ≥2 路可独立停 | 降级符合策略 |
| D. 配置驱动 | 阈值/周期走 config | 改参不重编 |
| E. 真机 | §6 模块接入 HI | 与 Mock 同话题 |
| F. 面板 | web_console 农业站页 | 浏览器可演示 |

## 6. 确定传感清单（可采购真机）

原则：数字或 **I2C ADC** 模块；淘宝易购；避免「只能模拟口直出且无 ADC」的裸探针当作唯一方案。

### 6.1 官方推荐套装

| 优先级 | 物理量 | **指定模块** | 接口 | Topic | 量级（¥） |
|--------|--------|--------------|------|-------|-----------|
| **必选** | 空气温湿度 | **AHT20**（或 SHT30） | I2C | `/agri/air/temperature`、`.../humidity` | 8–35 |
| **必选** | 光照 | **BH1750** | I2C | `/agri/light/illuminance` | 5–20 |
| **必选** | 土壤墒情 | **电容土壤湿度探针** + **ADS1115**（I2C ADC 模块） | I2C | `/agri/soil/moisture` | 25–60 |
| 可选 | 温室 CO₂ | **SCD40** / SCD41 | I2C | `/agri/air/co2` | 80–180 |
| 可选 | 土壤温度 | **DS18B20** 防水探头 | 1-Wire | `/agri/soil/temperature` | 5–15 |

**默认真机三件套（锁定）：AHT20 + BH1750 +（电容土壤探针 + ADS1115）。**  
I2C 设备地址不同，可由 **一个 HI 进程独占总线**，或拆成「气候+光照」与「土壤 ADC」两个 package（仍建议单写者规则写进实现说明）。

不纳入默认：

- 室内 **PMS5003**（偏城市空气，非农业主叙事；个人可加）。  
- **DHT11**、裸 **MQ** 模拟气体。  
- 昂贵 RS485 墒情/NPK 一体杆（可作进阶，不作入门必选）。

### 6.2 站端其它物料

| 类别 | 推荐 | 量级（¥） |
|------|------|-----------|
| SBC + 存储 | Pi / Radxa / 香橙派 + microSD | 100–350 |
| 电源 | 5 V  ent足适配器；田间场景可后期加太阳能，第一版插电 | 20–50 |
| 外壳 | 防水接线盒、探针引出孔 | 30–100 |
| **传感三件套** | | **约 40–120** |
| **含 SBC 整站** | | **约 200–600** |

### 6.3 验收约定

| 项 | 约定 |
|----|------|
| Mock | 同名 Topic，字段与真机一致 |
| 驱动顺序 | ① AHT20 ② BH1750 ③ 土壤+ADS1115 ④ 可选 SCD40 |
| 真机最低 | **至少两路实物**（建议 AHT20 + 土壤 或 AHT20 + BH1750） |
| 采购 | 模块版 + 排针；ADS1115 买现成模块；探针插入花盆即可室内演示「农业」 |

## 7. 与其它 Demo 的边界

| Demo | 本站不做什么 |
|------|----------------|
| Turtle | 不重复平面遥操作 |
| Rover | 不上底盘/激光；告警不是运动急停 |
| 可穿戴 / 手机外设 | 产品形态明确为 Linux 边缘站 |

## 8. 命名与迁移

| 现名 / 曾用名 | 目标名 |
|---------------|--------|
| `smart_device_demo` | `apps/agri_station_demo` |
| `smart_device.launch.yaml` | `configs/agri_station.launch.yaml` |
| 曾用 Env Station / `/env/...` | Agri Station / `/agri/...` |

## 9. 成功标准（MVP）

1. launch：App + ≥2 路传感（Mock）+ health + config + web_console。  
2. 可见温湿/光照/土壤（或子集）与超阈告警。  
3. 停一路传感 → 降级且其余仍更新。  
4. 与 Turtle / Rover 同仓可切换产品包。
