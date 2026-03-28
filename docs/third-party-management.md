# 第三方库管理

本文说明 OpenEmber 如何统一获取、缓存与构建第三方源码，以及如何与 Kconfig / CMake 联动。**版本号与下载 URL 的唯一事实来源**为 `cmake/Dependencies.cmake`；业务代码不应手写版本字符串。

## 设计要点（结论摘要）

| 项目 | 约定 |
|------|------|
| 版本与 URL | 仅在 `cmake/Dependencies.cmake` 钉死；升级只改该处并重新配置。 |
| 本地缓存 | `third_party/<cache-key>.tar.gz` 或 `.zip`；FETCH 模式下**先查缓存再联网**，下载仍落盘到 `third_party/`。 |
| 构建树路径 | `build/_deps/<上游解压顶层目录名>/`，与 GitHub 等官方归档一致（**不再**使用 FetchContent 的 `openember_*-src` 风格目录名）。 |
| Kconfig | `third_party/Kconfig`：**全部 `OPENEMBER_THIRD_PARTY_BUNDLE_*` 列出**；必选项由 `OPENEMBER_TP_LINK_*` + `select` 锁定；可选预取见 `openember_third_party_prefetch_unused_bundles()`。 |
| 默认行为 | 未出现在 `.config` 中的 bundle 符号由 `cmake/OpenEmberThirdPartyBundleDefaults.cmake` 视为 **ON**（与历史「默认拉取」一致）。 |
| 生成 CMake 缓存 | `scripts/kconfig/genconfig.sh` 将 Kconfig 选项写入 `build/config.cmake`（`FORCE`）。 |

实现辅助：`cmake/ThirdPartyArchive.cmake`（解压/下载）、各 `cmake/Get*.cmake`（`openember_third_party_prepare_stage` + `add_subdirectory`）。

## 模式：`OPENEMBER_THIRD_PARTY_MODE`

- **FETCH**：优先 `third_party/` 已有归档；否则下载到 `third_party/` 再解压到 `build/_deps/...`。
- **VENDOR**：将归档放入 `third_party/`，或设置各库对应的 `*_LOCAL_SOURCE` 指向已解压目录。
- **SYSTEM**：使用系统已安装的包（企业/Yocto 等）；不下载。若某库在 SYSTEM 下未找到，配置阶段报错。

## 官方维护的第三方清单（插件式管理）

下列库均由项目 **钉版本、提供 Get 脚本与（适用时）BUNDLE 开关**，可按需启用；**未在下列单独列「何时解析」的**，在根 `CMakeLists.txt` 的 `openember_third_party_resolve_*` / `openember_transport_resolve_*` 中有明确调用条件。

| 上游 | CMake 版本变量（节选） | 脚本 | 典型启用条件 / 备注 |
|------|------------------------|------|---------------------|
| [zlog](https://github.com/HardySimpson/zlog) | `OPENEMBER_ZLOG_VERSION` | `GetZlog.cmake` | `OPENEMBER_LOG_BACKEND=ZLOG` |
| [cJSON](https://github.com/DaveGamble/cJSON) | `OPENEMBER_CJSON_VERSION` | `GetCjson.cmake` | `OPENEMBER_JSON_LIBRARY=CJSON` |
| [nlohmann/json](https://github.com/nlohmann/json) | `OPENEMBER_NLOHMANN_JSON_VERSION` | `GetNlohmannJson.cmake` | `OPENEMBER_JSON_LIBRARY=NLOHMANN_JSON` |
| [SQLite amalgamation](https://www.sqlite.org/) | `OPENEMBER_SQLITE_*` | `GetSqlite.cmake` | 全局解析（config 等使用） |
| [yaml-cpp](https://github.com/jbeder/yaml-cpp) | `OPENEMBER_YAMLCPP_VERSION` | `GetYamlCpp.cmake` | `OPENEMBER_WITH_YAMLCPP=ON` |
| [Asio](https://github.com/chriskohlhoff/asio) standalone | `OPENEMBER_ASIO_TAG` | `GetAsio.cmake` | `OPENEMBER_WITH_ASIO=ON` |
| [spdlog](https://github.com/gabime/spdlog) | `OPENEMBER_SPDLOG_VERSION` | `GetSpdlog.cmake` | `OPENEMBER_LOG_BACKEND=SPDLOG` |
| [Eclipse Paho MQTT C](https://github.com/eclipse-paho/paho.mqtt.c) | `OPENEMBER_PAHO_MQTT_C_VERSION` | `GetPahoMqttC.cmake` | `OPENEMBER_COMPONENT_MQTT=ON` |
| [Mongoose](https://github.com/cesanta/mongoose) | `OPENEMBER_MONGOOSE_VERSION` | `GetMongoose.cmake` | web_dashboard 等 |
| [NNG](https://github.com/nanomsg/nng) | `OPENEMBER_NNG_VERSION` | `GetNng.cmake` | `OPENEMBER_MSGBUS_USE_NNG=ON` |
| [LCM](https://github.com/lcm-proj/lcm) | `OPENEMBER_LCM_VERSION` | `GetLcm.cmake` | `OPENEMBER_MSGBUS_USE_LCM=ON` |
| [libzmq](https://github.com/zeromq/libzmq) | `OPENEMBER_LIBZMQ_VERSION` | `GetLibzmq.cmake` | msgbus ZMQ 后端选用时 |
| [cppzmq](https://github.com/zeromq/cppzmq) | `OPENEMBER_CPPZMQ_VERSION` | `GetCppZmq.cmake` | 同上 |
| [ruckig](https://github.com/pantor/ruckig) | `OPENEMBER_RUCKIG_VERSION` | `GetRuckig.cmake` | `OPENEMBER_WITH_RUCKIG=ON`（默认关；C++20 库） |

> 链接业务目标时按需 `target_link_libraries(... yaml-cpp::yaml-cpp)`、`target_link_libraries(... ruckig::ruckig)` 等；并非所有目标都会自动链接全部可选库。

## Kconfig：`Third party (bundles)`

- 菜单在 **`components/Kconfig` 与 `modules/Kconfig` 之后** 加载，以便 `OPENEMBER_LOG_BACKEND_*`、`OPENEMBER_JSON_LIBRARY_*`、`OPENEMBER_MSGBUS_BACKEND_*` 等符号已定义。
- **清单**：`Dependencies.cmake` 中钉死的第三方均在菜单中列出（不再用 `depends on` 隐藏条目）。
- **必选**：与当前功能绑定的库由隐藏的 `OPENEMBER_TP_LINK_*` 配置通过 **`select`** 强制勾选；在 `menuconfig` 中表现为 **`<*>`（built-in）且不可取消**。
- **可选**：未绑定当前功能的库默认为关；若手动勾选，则在 **FETCH/VENDOR** 下仅 **预取**（下载到 `third_party/`、解压到 `build/_deps/`），**不参与** 对应 `add_subdirectory` 编译，除非后续在别处启用该功能。实现见 `openember_third_party_prefetch_unused_bundles()`。
- **SYSTEM**：不预取；仍按各 `resolve_*` 使用系统包。
- 详见 `third_party/Kconfig` 与 `scripts/kconfig/genconfig.sh`。

## 目录约定

| 路径 | 含义 |
|------|------|
| `third_party/<cache-key>.tar.gz` \| `.zip` | 离线缓存（FETCH 下载落盘位置） |
| `build/_deps/<上游顶层目录>/` | 解压后的源码树 |
| `build/_deps/<上游顶层目录>-build/` | `add_subdirectory` 的二进制目录 |

## 升级流程（简要）

1. 在 `cmake/Dependencies.cmake` 中更新版本与 URL（及必要时 `*_CACHE_KEY` / `*_STAGE_DIR_NAME`）。
2. 更新或删除 `third_party/` 中旧归档后重新 `cmake`。
3. 全量编译与链接验证。

## 日志后端（`OPENEMBER_LOG_BACKEND`）

| 值 | 说明 |
|----|------|
| `ZLOG` | 默认；`zlog` + `LOG_FILE` |
| `SPDLOG` | C++ `spdlog` |
| `BUILTIN` | 自研轻量实现 |

生成头：`${CMAKE_BINARY_DIR}/include/ember_log_backend.h`。

## JSON（`OPENEMBER_JSON_LIBRARY`）

- `CJSON` / `NLOHMANN_JSON`：宏见 `ember_json_config.h`。

## 代码接入原则

- 仅引入当前能力所需的最小依赖集合。
- 通过 CMake 变量或导入目标暴露 include/link，避免在核心代码硬编码路径。
