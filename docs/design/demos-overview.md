# OpenEmber 内置 Demo 总览

- 更新日期：2026-10-28
- 目的：三档产品 Demo 对照入口；细节不在此重复。

同一工程通过 **Kconfig / defconfig + `configs/*.launch.yaml`** 打出不同软件包；`system/` 与 `services/` 按能力复用，不按产品复制。

## 对照表

| Demo | App（现 / 规划） | 故事 | 状态 | 设计 | 用户文档 |
|------|------------------|------|------|------|----------|
| **Turtle** | `apps/turtle_demo` | 平面通信入门（Link + msgs + launch；类 turtlesim） | 已落地 | （以用户文档为准，不单独立项） | openember-docs `docs/turtle-demo.md` |
| **Rover** | `apps/rover_demo` | 差速巡游小车：执行、传感、急停；仿 + DIY ≤¥3000 | 宿主机 MVP 已落地（sim/teleop） | [rover-demo.md](./rover-demo.md) | openember-docs `docs/rover-demo.md` |
| **Agri Station** | `apps/agri_station_demo`（`smart_device_demo` 仍保留） | 智慧农业监测站：多传感、告警/降级、运维面 | 宿主机 Mock MVP 已落地 | [agri-station-demo.md](./agri-station-demo.md) | openember-docs `docs/agri-station-demo.md` |

## 分工（避免重叠）

| | Turtle | Rover | Agri Station |
|--|--------|-------|--------------|
| 主轴 | 多节点消息闭环 | 运动 + 安全 | 弱实时监测 + 运维 |
| 真机 | 不需要 | 底盘 + 激光等 | AHT20 / BH1750 / 土壤+ADS1115 等 |
| 不做什么 | 不上完整底盘与农业站 | 不做 SLAM 发行、不做双足 | 不做底盘、不做可穿戴 |

## 共用中间层（按 launch 裁剪）

`launch_manager`、`health_monitor`、`config_service`、`device_manager`、可选 `web_console` / `host_monitor` / `hardware_interface` / `ota_agent` 等。

相关架构：[Application Layer](../architecture/application-layer-design-guide.md)、[Runtime Supervision](../architecture/runtime-supervision-and-product-app-design.md)、[Hardware Interface](./hardware-interface.md)。
