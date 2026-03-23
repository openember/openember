# 第三方库管理（FetchContent / Vendor / System）

## 模式定义

通过 `OPENEMBER_THIRD_PARTY_MODE` 控制第三方库来源：

- `FETCH`：使用 CMake `FetchContent` 下载并构建“固定版本”的依赖
- `VENDOR`：使用本地已解压源码（默认路径为 `download/_extracted/<name>-<version>/`，与 `Dependencies.cmake` 中变量一致）
- `SYSTEM`：优先使用系统已安装的依赖（Yocto/企业场景）

> 仓库内不再提交 `cJSON` / `zlog` / `paho.mqtt.c` / `sqlite` 等上游源码树；见 `third_party/README.md`。

## 版本钉死与升级

每个依赖应在 CMake 中用“版本变量 + URL/下载信息”固定到可追溯状态。

升级流程（建议）：

1. 在 `cmake/Dependencies.cmake` 中更新依赖版本变量（以及必要时的校验信息）
2. 重新配置构建：`cmake .. -DOPENEMBER_THIRD_PARTY_MODE=FETCH`
3. 验证目标：编译通过、目标链接正确（include dir / library name）
4. 若升级涉及 ABI 行为变化，更新对应的 OpenEmber 适配层/文档说明

## 当前已接入的依赖（示例范围）

- `zlog`（固定到 `1.2.16`：Fetch/Vendor/System；脚本见 `cmake/GetZlog.cmake`；上游无 CMake，由 `cmake/vendor/zlog` 包装编译 `FetchContent` 拉取的 `src/`）
- `cJSON`（固定到 `1.7.15`；Fetch/Vendor/System；`cmake/GetCjson.cmake`）— 与 **`nlohmann/json`** 二选一，由 `OPENEMBER_JSON_LIBRARY`（`CJSON` / `NLOHMANN_JSON`）控制；见 `inc/ember_json_config.h.in` 生成的 `ember_json_config.h`
- `nlohmann/json`（默认版本 `3.11.3`；`cmake/GetNlohmannJson.cmake`；目标 `nlohmann_json::nlohmann_json`）
- `paho.mqtt.c`（固定到 `1.3.13`；Fetch/Vendor/System；`cmake/GetPahoMqttC.cmake`）
- `sqlite`（amalgamation，与历史 `3.39.2` 对齐；Fetch/Vendor/System；`cmake/GetSqlite.cmake`）
- `yaml-cpp`（可选：`OPENEMBER_WITH_YAMLCPP`；默认 ON；Fetch/Vendor/System；`cmake/GetYamlCpp.cmake`；目标 `yaml-cpp::yaml-cpp`）
- `Asio` standalone（可选：`OPENEMBER_WITH_ASIO`；默认 ON；Fetch/Vendor/System；`cmake/GetAsio.cmake`；目标 `openember::asio`）
- `spdlog`（仅当 `OPENEMBER_LOG_BACKEND=SPDLOG` 时拉取；Fetch/Vendor/System；`cmake/GetSpdlog.cmake`；目标 `spdlog::spdlog`）
- （已删除）`easylogger`：当前已不再维护

## 日志后端（`OPENEMBER_LOG_BACKEND`）

通过 CMake 缓存变量选择实现，**不**在 `ember_config.h` 里手写互斥宏：

| 值 | 说明 |
|----|------|
| `ZLOG` | 默认；沿用 `zlog` + `LOG_FILE`（`/etc/openember/zlog.conf`） |
| `SPDLOG` | C++ `spdlog`，彩色控制台输出（`libs/Log/log_spdlog.cpp`） |
| `BUILTIN` | 自研轻量实现：stderr + 前缀，无第三方日志库 |

配置阶段会生成 `${CMAKE_BINARY_DIR}/include/ember_log_backend.h`，`inc/ember_config.h` 通过 `#include "ember_log_backend.h"` 接入。

可选依赖（yaml-cpp / Asio）**不会**自动加入所有目标的链接行；需要时在模块 `target_link_libraries(... yaml-cpp::yaml-cpp)` / `target_link_libraries(... openember::asio)`。

### JSON 库（`OPENEMBER_JSON_LIBRARY`）

- `CJSON`（默认）：C API，适合与现有 C 代码混用。
- `NLOHMANN_JSON`：仅适用于使用 `<nlohmann/json.hpp>` 的 **C++** 源码；`OPENEMBER_JSON_USE_*` 宏在 `ember_json_config.h` 中定义，业务代码用 `#if OPENEMBER_JSON_USE_CJSON` / `OPENEMBER_JSON_USE_NLOHMANN_JSON` 分支。

## 代码接入原则

为避免维护成本膨胀：

- 只为关键能力引入 transport / ABI 适配层所需的最低集合（不强依赖上游测试/示例/语言绑定）
- CMake 层提供统一的 include/lib 变量（例如 `ZLOG_INCLUDE_DIRS`、`ZLOG_LIBRARIES`），让核心代码保持稳定
