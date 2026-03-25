# OpenEmber 构建方法（Kconfig + CMake）

本文档给出当前推荐流程：先通过 Kconfig 生成 `build/config.cmake`，再由 CMake 读取配置完成构建。

## 1. 前置条件

- Linux 环境
- CMake（项目当前最小版本 `3.1.0`）
- GCC/G++ 或兼容编译器

> Kconfig 工具链采用 Linux 原生 `kconfig-frontends`。  
> 项目脚本会自动下载并解压 `kconfig-frontends-nox` 到仓库本地目录 `.kconfig-frontends/`（无需 root 安装）。

仓库根目录仅保留顶层 `Kconfig`；各层选项分文件位于 `apps/Kconfig`、`modules/Kconfig`、`components/Kconfig`、`core/Kconfig`、`platform/Kconfig`（`menuconfig.sh` 会按相同相对路径复制到构建目录供工具解析）。

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

## 2.1 方案一：使用 `make` 包装命令（新增）

根目录已提供 `Makefile`，可将多条命令简化为：

```bash
# 菜单配置（交互）
make menuconfig

# 生成 build/config.cmake
make genconfig

# 配置 + 编译
make configure
make build
```

或一步到位：

```bash
make all
```

可选参数：

```bash
make all BUILD_DIR=build-dev JOBS=16
```

## 2.2 方案二：使用 `ember` 命令（新增）

新增脚本：`scripts/ember`。可直接执行：

```bash
./scripts/ember menuconfig
./scripts/ember genconfig
./scripts/ember configure
./scripts/ember build
```

其中 `./scripts/ember build` 会自动补齐流程：

- 若 `build/.config` 不存在：自动以非交互默认配置生成
- 若 `build/config.cmake` 不存在：自动执行 `genconfig`
- 然后自动执行 `cmake -S . -B build` + `cmake --build build -j$(nproc)`

可选：

```bash
# 指定构建目录
./scripts/ember build build-dev

# 环境变量控制
OPENEMBER_BUILD_DIR=build-dev OPENEMBER_JOBS=16 ./scripts/ember build

# 安装到当前用户（推荐，默认）
./scripts/ember install

# 自定义安装目录（例如系统目录）
./scripts/ember install --prefix /usr/local/bin
```

说明：

- `ember install` 默认安装到 `~/.local/bin/ember`，并自动写入 `~/.bashrc`：
  - 自动补齐 PATH（若缺失）
  - 自动加载补全（新开终端生效）
- 若使用 `--prefix` 且目录需要权限，脚本会在必要时自动调用 `sudo` 完成软链接写入。
- 不建议使用 `sudo ./scripts/ember install ...` 直接整条提权执行。

### 2.2.1 命令补全

安装后一般无需手动配置补全；若只想临时在当前 shell 启用，可执行：

```bash
source <(./scripts/ember completion bash)
```

如果 ember 在 PATH（例如软链接到 `/usr/local/bin/ember`），也可以：

```bash
source <(ember completion bash)
```

卸载：

```bash
# 自动探测并删除 PATH 中的 ember、默认路径及常见系统路径中的 ember
./scripts/ember uninstall

# 额外指定一个前缀目录参与删除
./scripts/ember uninstall --prefix /usr/local/bin
```



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
  - 可单独启停：`system/launch_manager` / `examples/hello_node` / `system/log_service` / `system/device_manager` / `system/config_service` / `system/health_monitor` / `services/ota_update_service` / `references/sensor_data_reference` / `services/web_dashboard`；示例还可选 `examples/pubsub_two_nodes`、`examples/msgbus_nng_forwarder`
  - 映射到 `OPENEMBER_MODULE_*`，由 `apps/CMakeLists.txt` 条件 `add_subdirectory()` 控制

## 4. 运行说明

构建产物通常位于 `build/bin/`：

- `launch_manager`
- `hello_node`
- `pubsub_publisher` / `pubsub_subscriber`（启用 `OPENEMBER_EXAMPLE_PUBSUB_TWO_NODES` 且 pub/sub 后端为 ZMQ/NNG/LCM 时）
- `msgbus_nng_forwarder`（启用 `OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER` 且内部 msgbus 为 NNG 时）

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

