# OpenEmber 构建方法（Kconfig + CMake）

本文档给出当前推荐流程：先通过 Kconfig 生成 `build/config.cmake`，再由 CMake 读取配置完成构建。

## 1. 前置条件

- Linux 环境
- CMake（项目当前最小版本 `3.1.0`）
- GCC/G++ 或兼容编译器

> Kconfig 工具链采用 Linux 原生 `kconfig-frontends`。  
> 项目脚本会自动下载并解压 `kconfig-frontends-nox` 到仓库本地目录 `.kconfig-frontends/`（无需 root 安装）。

## 2. 一键推荐流程（不传 `-DXXX`）

在工程根目录执行：

```bash
# 1) 进入 menuconfig（交互式）
bash scripts/kconfig/menuconfig.sh build

# 如果环境不支持交互 TUI（如 CI / 无 TTY），使用默认配置生成 .config：
# OPENEMBER_KCONFIG_NONINTERACTIVE=1 bash scripts/kconfig/menuconfig.sh build

# 2) 由 .config 生成 build/config.cmake
bash scripts/kconfig/genconfig.sh build

# 3) CMake 读取 build/config.cmake 并构建
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

说明：

- `CMakeLists.txt` 会自动 `include(${CMAKE_BINARY_DIR}/config.cmake)`（若存在）。
- `scripts/kconfig/genconfig.sh` 会把 Kconfig 选项映射成 CMake cache 变量（`FORCE`），实现“以 Kconfig 为主”。

## 3. 当前已支持的 Kconfig 配置项

`menuconfig` 已支持以下关键配置（无需再手工 `-D`）：

- **Build Options**
  - `TESTS_ENABLED`
  - `EXAMPLES_ENABLED`
  - `OPENMP_ENABLED`
  - `DEBUG_ENABLED`
  - `OPTIMIZATION_DISABLED`
  - `CROSSCOMPILE_ENABLED`
- **Logging**
  - `OPENEMBER_LOG_BACKEND`：`ZLOG` / `SPDLOG` / `BUILTIN`
  - `OPENEMBER_LOG_FILE`：zlog 配置路径（例如 `/etc/openember/zlog.conf`）
- **Dependencies**
  - `OPENEMBER_THIRD_PARTY_MODE`：`FETCH` / `VENDOR` / `SYSTEM`
  - `OPENEMBER_JSON_LIBRARY`：`CJSON` / `NLOHMANN_JSON`
  - `OPENEMBER_WITH_YAMLCPP`
  - `OPENEMBER_WITH_ASIO`
- **Transport / Msgbus**
  - Pub/Sub backend（三选一）：ZMQ / NNG / LCM（映射到 `BUILD_PUBSUB_*`）
  - Internal msgbus backend（二选一）：NNG / LCM（映射到 `OPENEMBER_MSGBUS_USE_*`）
- **Framework Modules（新增）**
  - 可单独启停：`Template` / `Alogd` / `DeviceManager` / `MessageDispatcher` / `ConfigManager` / `MonitorAlarm` / `OTA` / `Acquisition` / `WebServer`
  - 映射到 `OPENEMBER_MODULE_*`，由 `modules/CMakeLists.txt` 条件 `add_subdirectory()` 控制

## 4. 运行说明

构建产物通常位于 `build/bin/`：

- `Template`
- `msgbus_nng_forwarder`（当 msgbus 使用 NNG 时）
- `pubsub_publisher` / `pubsub_subscriber`（按 pub/sub backend 相关配置生成）

## 5. 常见问题

- **`Permission denied`（运行 kconfig 脚本）**
  - 已修复：`menuconfig.sh` 内部改为 `bash ensure-kconfig-frontends-nox.sh` 调用，不依赖执行位。
- **`Missing build/.config`**
  - 需要先执行 `menuconfig.sh` 生成 `.config`，再执行 `genconfig.sh`。
- **无交互环境无法打开菜单**
  - 使用：
    ```bash
    OPENEMBER_KCONFIG_NONINTERACTIVE=1 bash scripts/kconfig/menuconfig.sh build
    ```
    该模式会按默认值生成 `.config`。

