# 外部产品仓与 `ember menuconfig`（过渡说明）

- 日期：2026-10-29
- 状态：过渡；**完整目标方案**见 [product-scaffold-and-variants.md](./product-scaffold-and-variants.md)
- 相关：[demos-overview.md](./demos-overview.md)、[application-layer-design-guide.md](../architecture/application-layer-design-guide.md)

> 本文描述迁出初期的 `OPENEMBER_EXTERNAL_APPS_ROOT` 挂载方式。评审通过后的产品出生方式、menuconfig 分区与多型号配置以 **product-scaffold-and-variants** 为准。

## 1. 问题

主仓 `apps/` 堆完整产品故事会膨胀，且与「用户节点不进主仓」冲突。  
产品拆到 `openember-demo-*` 后，还能否继续用 **`ember menuconfig` → genconfig → build**？

## 2. 结论

**可以保留 menuconfig，但职责要切开：**

| 层 | 谁配置 | 工具 |
|----|--------|------|
| 框架能力（Link、msgs、system、services、HI、第三方 bundle） | 仍在 **openember** 树里 | **`ember menuconfig` / genconfig**（不变） |
| 产品行为（agri Qt、通道数、自有节点） | **产品仓** CMake/`configs` | 产品仓选项或以后产品侧薄 Kconfig |

`ember` 当前把「工程根」定义为「带 `CMakeLists.txt` + `scripts/kconfig/menuconfig.sh` 的目录」——那就是 **openember 本身**，不是 demo 仓。硬把 demo 仓扮成第二份完整 Kconfig 树，会复制脚本、双份 `.config`，维护成本高。

对标：

- **RT-Thread**：menuconfig 裁内核/组件；BSP `applications/` 是产品代码，不靠再造一套 OS Kconfig。  
- **FastAPI 模板**：模板拷走魔改；真正持续升级的是 pip 里的库，不是整份模板 merge。

## 3. 推荐集成方式（当前落地）

```text
openember-demo-agri-station/     # 或 rover
  apps/…                         # 产品源码 + CMakeLists
  configs/…                      # launch、fieldbus YAML
  openember.defconfig            # 建议的框架 .config 片段（文档/可选合并）
  scripts/build-with-openember.sh

openember/                       # 子模块、兄弟目录或 OPENEMBER_DIR
  ember menuconfig               # 只配框架
  cmake -DOPENEMBER_EXTERNAL_APPS_ROOT=<demo根>
```

- `OPENEMBER_EXTERNAL_APPS_ROOT`：指向产品仓根；openember 在编完自身 `apps/`（仅 turtle 等）后 `add_subdirectory` 该根目录。  
- 产品仓顶层 `CMakeLists.txt` 只 `add_subdirectory(apps/…)`，继续用 `OPENEMBER_ADD_EXECUTABLE` / `openember::core`（仍在 openember 的 CMake 工程内编译）。  
- 主仓 **不再** 默认编译 agri/rover；Kconfig 去掉对应开关。

这样：

1. **menuconfig 体验保留**（框架裁剪仍是一条命令）。  
2. **产品仓结构 = 用户产品仓结构**（可直接参考）。  
3. 不需要在 demo 根再维护一份 kconfig-frontends 拷贝。

## 4. 不推荐（现阶段）

| 做法 | 原因 |
|------|------|
| demo 根再实现完整 menuconfig 树 | 与框架脚本双份；`ember add` 语义混乱 |
| 仅 `add_subdirectory(openember)` 且以 demo 为 `project()` | openember 大量路径按「自身为源码根」假设，易碎 |
| 整仓 fork openember 再只改 apps | 回到「无法干净升级框架」 |

## 5. 后续可增强（非阻塞）

1. `ember build --product /path/to/demo`：自动传入 `OPENEMBER_EXTERNAL_APPS_ROOT` 并可选合并 `openember.defconfig`。  
2. 产品仓薄 Kconfig（仅 Qt、通道数），`source` 框架 Kconfig——等外部产品 ≥2 个稳定后再做。  
3. `find_package(OpenEmber)` 安装布局：产品完全脱离源码树编译（更像 SDK）。

## 6. 仓库命名

- `openember-demo-agri-station`  
- `openember-demo-rover`  

主仓只保留 **turtle**（及可选 `smart_device_demo`）作框架入门与 CI。
