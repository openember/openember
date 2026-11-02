# OpenEmber Demo 总览

- 更新日期：2026-10-29
- 目的：分清「框架内置入门」与「外部产品样板仓」；细节不在此重复。
- **产品脚手架 / 多型号 / menuconfig 分区（待评审）**：[product-scaffold-and-variants.md](./product-scaffold-and-variants.md)
- 过渡挂载：[external-product-repos.md](./external-product-repos.md)

同一 **openember** 工程通过 **Kconfig / defconfig** 裁剪框架能力；**完整产品故事**在独立仓库，经 `-DOPENEMBER_EXTERNAL_APPS_ROOT` 挂入同一次构建。

## 对照表

| Demo | 位置 | 故事 | 状态 |
|------|------|------|------|
| **Turtle** | 主仓 `apps/turtle_demo` | 平面通信入门（Link + msgs + launch） | 内置 |
| **Rover** | [openember-demo-rover](https://github.com/openember/openember-demo-rover) | 差速巡游：执行、传感、急停 | 外部产品样板 |
| **Agri Station** | [openember-demo-agri-station](https://github.com/openember/openember-demo-agri-station) | 智慧农业监测站：IO 卡 / 告警 / Qt 台 | 外部产品样板 |

## 分工

| | Turtle（主仓） | Rover / Agri（外部） |
|--|----------------|----------------------|
| 角色 | 框架回归与入门 | 用户产品工程范本 |
| 维护 | 随 openember 升级 | 钉框架版本；可独立魔改 |
| menuconfig | 配置 Link/system/services 等 | 仍对 **openember** 跑 `ember menuconfig`；产品选项在产品仓 |

## 共用中间层

`launch_manager`、`health_monitor`、`config_service`、`device_manager`、可选 `web_console` / `host_monitor` / `hardware_interface` 等仍由框架提供，产品 launch 按需裁剪。
