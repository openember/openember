# Msgbus 集成 Zenoh：分析与实施方案

本文说明 [Eclipse Zenoh](https://github.com/eclipse-zenoh/zenoh) 与 OpenEmber `components/msgbus` 的集成思路。Zenoh 核心为 **Rust** 实现，对外通过 **C / C++ / Python** 等绑定暴露 API；OpenEmber 仓库主体为 **C/C++**，Python 侧以**独立工具链**支持，不强制进入同一套 CMake 静态链接。

## 1. 上游仓库与职责划分

| 仓库 | 说明 | 与 OpenEmber 的关系 |
|------|------|---------------------|
| [eclipse-zenoh/zenoh](https://github.com/eclipse-zenoh/zenoh) | Rust 参考实现、协议与 `zenohd` 路由 | 不直接作为 CMake 依赖编译进应用；可作为**协议与行为**的权威参考 |
| [eclipse-zenoh/zenoh-c](https://github.com/eclipse-zenoh/zenoh-c) | **官方 C API**（Rust 绑定），提供 CMake、`find_package(zenohc)` | **嵌入式 / C / C++ 路径的首选**：与现有 `*_wrapper.c` + `msg_bus_*` 模型一致 |
| [eclipse-zenoh/zenoh-cpp](https://github.com/eclipse-zenoh/zenoh-cpp) | **仅头文件**的 C++17 API，依赖已安装的 **zenoh-c** 或 [zenoh-pico](https://github.com/eclipse-zenoh/zenoh-pico) | 可选：实现 `TransportBackend` 的「原生 C++」后端（类比 `lcm_transport_backend.cpp`） |
| [eclipse-zenoh/zenoh-python](https://github.com/eclipse-zenoh/zenoh-python) | PyPI `eclipse-zenoh`，Rust 扩展 | **不**与主工程同一 CMake 产物链接；通过 **pip/venv** 供脚本、网关、调试工具使用 |

## 2. 当前 OpenEmber msgbus 架构（集成锚点）

- **C 抽象**：各后端在 `*_wrapper.c` 中实现 `lcm_wrapper.h` 所声明的 `msg_bus_*`（见 `components/msgbus/lcm_wrapper.h` 及 `message.h` 的包含链）。
- **编译期单选**：通过 `EMBER_LIBS_USING_NNG` / `LCM` / `ZEROMQ` 等宏在编译期选定一种实现（`components/msgbus/CMakeLists.txt` 汇总各 wrapper）。
- **C++ 路径**：`MsgBusNode` → `CreateDefaultTransportBackend()` → 要么 **LCM 的 C++ 后端**（`lcm_transport_backend.cpp`），要么 **`ClibTransportBackend`** 调 `msg_bus_*`（`transport_backend_clib.cpp`）。

因此 Zenoh 的**最小可行集成**是：新增 **`zenoh_wrapper.c`**（及头文件），在 Kconfig/CMake 中选择 `OPENEMBER_MSGBUS_USE_ZENOH` 时定义 `EMBER_LIBS_USING_ZENOH` 并编译该 wrapper，与现有 NNG/LCM 并列。

可选第二步：增加 **`zenoh_transport_backend.cpp`**，在 `zenoh-cpp` + `zenoh-c` 已可用的前提下，走「纯 C++」实现（与 LCM C++ 路径对称）。

## 3. 语言路径建议

### 3.1 C / 嵌入式（与现有 `msg_bus_*` 一致）

- **依赖**：**zenoh-c** 构建产物（`zenohc::shared` 或 `zenohc::static`）。
- **集成方式**：在 `zenoh_wrapper.c` 中调用 `zenoh` C API，实现 `msg_bus_init` / `publish` / `subscribe` / 回调等。
- **构建注意**：上游 [zenoh-c](https://github.com/eclipse-zenoh/zenoh-c) 的 CMake 会驱动 **Cargo/Rust** 编译静态/动态库，因此 **主机与交叉编译环境需安装 Rust 工具链**（版本以 `zenoh-c` 的 `rust-toolchain.toml` / README 为准，通常 MSRV ≥ 1.75）。这与当前纯 C 的 NNG/LCM 依赖相比，**CI 与 Yocto/SDK 镜像需额外步骤**。

### 3.2 C++（框架 `MsgBusNode`）

- **路径 A（推荐先做）**：仍用 `ClibTransportBackend` + `zenoh_wrapper.c`，**零新增 C++ 绑定依赖**，与今天 NNG 路径一致。
- **路径 B（可选）**：引入 **zenoh-cpp**，实现独立 `TransportBackend` 子类，在 `transport_backend_factory.cpp` 中按宏选用（类似 LCM）。需先解决 **zenoh-c** 的安装与 `find_package(zenohcxx)`，且注意 **C++17** 与 OpenEmber 组件标准一致。

### 3.3 Python（「支持三种语言」的落点）

- **不作为**主仓库单一 `cmake --build` 产物中的嵌入式解释器扩展。
- **推荐**：文档化 **venv + `pip install eclipse-zenoh`**（见 [zenoh-python](https://github.com/eclipse-zenoh/zenoh-python)），用于：
  - 与运行中的 OpenEmber 进程通过 **Zenoh 网络**互操作（发布/订阅同一 session/router）；
  - 运维脚本、仿真、与 ROS2/其他中间件桥接。
- 若需「随仓库可复现的 Python 版本」，可在 `requirements-zenoh.txt` 或 `docs` 中钉 **eclipse-zenoh** 版本，与 `cmake/Dependencies.cmake` 中 zenoh-c 版本**尽量对齐**（同一 Zenoh 主版本线），避免协议/会话不兼容。

## 4. 语义差异：Topic 与 Zenoh Key Expression

现有 `msg_bus_*` 使用 **字符串 topic / topic 前缀**（如 `openember/...`）。Zenoh 使用 **key expression**（层级与 `**` 等通配），并区分 **pub/sub、queryable、storage** 等。

集成时需要**约定一层映射**（在 wrapper 内实现）：

- 将 OpenEmber 的 `topic` / `topic_prefix` 映射为 Zenoh 的 key（例如前缀订阅用 `topic_prefix/**` 或项目内固定根 key + 子路径）。
- 二进制 `publish_raw` 需对应 Zenoh 的 payload 编码（默认不透明字节即可，若要与非 OpenEmber 节点互通需文档化编码）。

建议在实现 `zenoh_wrapper.c` 前单独列一张 **topic ↔ key expr** 表，并写入 `docs/components/msgbus.md`（或本文件附录）。

## 5. 第三方与 CMake 管理（与 OpenEmber 策略对齐）

- 在 `cmake/Dependencies.cmake` 中钉 **zenoh-c** 版本与 GitHub **tag 源码包 URL**（`.tar.gz`）。
- 新增 `cmake/GetZenohC.cmake`：`openember_third_party_prepare_stage` + `add_subdirectory` 或 `ExternalProject`（以上游实际 CMake 导出目标为准）。
- `third_party/Kconfig` 增加 **`OPENEMBER_THIRD_PARTY_BUNDLE_ZENOH_C`**（或命名一致），在选中 Zenoh 后端时 `select` 强制 bundle；可选预取逻辑可复用 `openember_third_party_prefetch_unused_bundles()` 模式。
- **环境变量**：构建 zenoh-c 前需 `cargo` 可用；可在 `docs/build.md` 增加「启用 Zenoh 后端需安装 Rust」说明。

**说明**：zenoh-c 构建链复杂，**SYSTEM** 模式可优先支持「发行版已安装 `zenohc`」的 `find_package(zenohc)`，减少从零编译频率。

## 6. Kconfig / CMake 开关（建议）

- `modules/Kconfig` 的 `choice OPENEMBER_MSGBUS_BACKEND_*` 中增加 **Zenoh**（与现有 LCM/NNG/ZMQ/UDP **互斥**，保持「单进程一种后端」）。
- `scripts/kconfig/genconfig.sh` 映射：
  - `OPENEMBER_MSGBUS_USE_ZENOH=ON`，其余 `USE_NNG`/`USE_LCM` 等为 OFF。
- 根 `CMakeLists.txt`：在解析第三方后，若启用 Zenoh，则 `openember_transport_resolve_zenoh_c()`（或并入现有 transport 解析函数）。

## 7. 分阶段实施方案

| 阶段 | 内容 | 产出 |
|------|------|------|
| **0** | 文档与设计（本文） | 共识与 topic 映射原则 |
| **1** | `Dependencies` + `GetZenohC` + Kconfig bundle + 可选 `find_package` SYSTEM | 配置阶段可拉取/找到 zenoh-c |
| **2** | `zenoh_wrapper.c` + `EMBER_LIBS_USING_ZENOH`，接入 `message.h` 包含链 | C/C++ 应用与 `MsgBusNode`（Clib 路径）可用 Zenoh |
| **3** | 示例与测试：`msgbus` 双进程或 `ctest` 覆盖 Zenoh session | 回归与文档更新 |
| **4**（可选） | `zenoh-cpp` + `zenoh_transport_backend.cpp` | 与 LCM 类似的 C++ 原生后端 |
| **5**（可选） | 仓库内 `examples/` 或 `tools/` 下 Python 样例 + `requirements` 片段 | 三方语言中的 Python 支持（进程外） |

## 8. 风险与缓解

| 风险 | 缓解 |
|------|------|
| Rust/Cargo 构建时间长、CI 负担 | 缓存 `third_party` 与 `CARGO_HOME`；优先 SYSTEM 包；文档明确可选组件 |
| 交叉编译 | 使用 zenoh-c 文档中的交叉编译选项；在目标 SDK 中预装对应 Rust target |
| 与 NNG/LCM 行为差异（发现、QoS） | 文档说明部署需 **zenohd** 或 peer-to-peer 模式；必要时仅支持 TCP/UDP 等子集先落地 |
| 版本漂移 | 与 PyPI `eclipse-zenoh` 主版本对齐策略写进 `docs/third-party-management.md` 表格 |

## 9. 参考链接

- Zenoh 主仓库：[eclipse-zenoh/zenoh](https://github.com/eclipse-zenoh/zenoh)
- C API：[eclipse-zenoh/zenoh-c](https://github.com/eclipse-zenoh/zenoh-c)
- Python：[eclipse-zenoh/zenoh-python](https://github.com/eclipse-zenoh/zenoh-python)
- C++：[eclipse-zenoh/zenoh-cpp](https://github.com/eclipse-zenoh/zenoh-cpp)
- 官网：[zenoh.io](https://zenoh.io)

---

*文档版本：与仓库主分支同步维护；实施时请以 `cmake/Dependencies.cmake` 中实际钉版本为准。*
