# OpenEmber 产品工程、扩展模型与版本升级方案

- 状态：Accepted（目标方案；实现分阶段落地）
- 日期：2026-11-02
- 适用范围：OpenEmber 主仓、`ember` CLI、产品脚手架、官方 demo 与用户产品仓
- 相关文档：
  - [external-product-repos.md](./external-product-repos.md)（当前 `OPENEMBER_EXTERNAL_APPS_ROOT` 过渡方案）
  - [application-layer-design-guide.md](../architecture/application-layer-design-guide.md)
  - [runtime-supervision-and-product-app-design.md](../architecture/runtime-supervision-and-product-app-design.md)
  - [demos-overview.md](./demos-overview.md)

## 1. 要解决的问题

OpenEmber 需要同时满足以下目标：

1. 作为开源项目独立维护、发布和演进，不被任何具体产品绑架。
2. 用户可以在几分钟内生成一个可编译、可运行、可部署的产品工程。
3. 产品可以大量定制应用、消息协议、领域模块、硬件驱动、UI 和部署拓扑。
4. 产品默认不修改 OpenEmber 核心，因此未来可以提升 OpenEmber 版本。
5. 对确实需要深度修改框架的用户，仍允许复制完整源码并独立演进。
6. 同一产品线可以维护多个型号，不为每个型号复制一套源码。

这些目标不能由单一的“复制整仓”或“只提供 SDK”同时完美满足。最终方案必须明确区分：

- **模板负责出生**；
- **产品仓负责业务**；
- **版本化核心负责持续升级**；
- **完整源码副本负责最大自由度，但不承诺低成本升级**。

## 2. 设计原则

1. **依赖方向始终从产品指向 OpenEmber**，OpenEmber 不长期反向加载产品源码。
2. **产品仓是真正的工程根**，拥有顶层 CMake、Kconfig、构建目录和部署配置。
3. **产品只依赖公开契约**：namespaced CMake targets、公开头文件、稳定配置 schema 和消息协议。
4. **产品消息是一等公民**，不要求所有消息都进入 `openember-msgs`。
5. **硬件驱动通过公开接口和注册机制接入**，不修改 Hardware Interface 内部工厂代码。
6. **业务逻辑、部署方式和运行时实现分离**。
7. **Kconfig 决定编进镜像的能力，YAML 决定运行时实例与拓扑**。
8. **升级核心与升级脚手架分开处理**；脚手架不会覆盖用户业务代码。

### 2.1 参考项目带来的取舍

| 项目 | 借鉴内容 | 不直接复制的内容 |
|---|---|---|
| RT-Thread | BSP/产品工程作为入口、Kconfig 包含核心、独立源码依赖与 `dist` 双模式 | 不照搬 SCons 和 MCU BSP 组织细节 |
| FastAPI Full Stack Template | 模板一次性生成完整起点；依赖更新与模板更新分离 | 不承诺自动合并被大量修改的模板源码 |
| ROS 2 | package 工作区、underlay/overlay、自定义 interface package、硬件插件 | 不立即引入 ament、colcon、package.xml 和发行农场 |
| AimRT | Module/Node/Pkg/Plugin/Protocol 分层、App/Pkg 双部署模式、轻量接口层 | 不在第一阶段全面动态插件化 |

## 3. 总体模型

OpenEmber 采用四层模型：

```text
① OpenEmber Core / SDK
   运行时、Link、Node、系统服务、公共组件、Hardware API、构建契约

② OpenEmber Interfaces
   openember-msgs、公共 schema、type support、兼容性规则

③ Product Scaffold
   ember new、目录模板、示例包、CI、构建与部署接线

④ Product Workspace
   用户应用、产品消息、领域模块、硬件驱动、型号和部署配置
```

依赖关系：

```text
Product Workspace
  ├── Product Interfaces
  ├── Product Modules
  ├── Product Drivers
  ├── Product Apps / Bringup
  └── OpenEmber public SDK
          └── OpenEmber Interfaces
```

禁止形成 `OpenEmber Core → 某个客户产品源码` 的依赖。

## 4. 三种用户模式

### 4.1 Product Scaffold 模式（默认推荐）

```bash
ember new acme-rover
cd acme-rover
ember build --product rover-a
```

- 产品仓独立；
- OpenEmber 通过 submodule、源码依赖或安装 SDK 提供；
- 产品默认不修改 OpenEmber；
- 可以通过提升锁定版本更新核心；
- 脚手架生成后由产品团队拥有，不要求持续同步模板源码。

这是官方 demo 和大多数用户产品的标准模式。

### 4.2 Standalone / Dist 模式

```bash
ember new acme-rover --standalone
# 或
ember dist --product rover-a
```

- 复制构建所需的 OpenEmber 源码和产品源码；
- 可离线交付、深度修改或移植；
- 可以直接把 OpenEmber 改造成自己的工程；
- 后续核心更新依靠 Git merge/rebase 或人工迁移；
- 不承诺任意魔改后仍可无冲突自动升级。

### 4.3 Installed SDK / Overlay 模式

```cmake
find_package(OpenEmber 1.2 CONFIG REQUIRED)
target_link_libraries(acme_rover_base PRIVATE openember::core)
```

```text
/opt/openember/1.2/       # underlay：安装后的 SDK/runtime
acme-rover/               # overlay：产品工作区
```

适合多产品共享 SDK、交叉编译、二进制交付和严格版本治理。

### 4.4 能力对比

| 模式 | 快速开始 | 修改核心 | 更新核心 | 典型用途 |
|---|---:|---:|---:|---|
| Product Scaffold | 快 | 默认不修改 | 容易 | 正式产品、官方 demo |
| Standalone / Dist | 最快 | 完全自由 | 人工合并 | 教学、离线交付、深度移植 |
| Installed SDK | 需要先安装 SDK | 不修改 | 最稳定 | 多产品、CI、量产环境 |

## 5. 产品工作区结构

### 5.1 推荐的大型产品布局

```text
acme-rover/
├── README.md
├── CMakeLists.txt                 # 产品工程根
├── Kconfig                       # 产品配置入口，包含 OpenEmber Kconfig
├── product.yaml                  # 脚手架与产品元数据
├── openember.lock                # OpenEmber、核心消息和工具版本
├── cmake/
│   └── ProductOptions.cmake
│
├── packages/
│   ├── rover_interfaces/         # 产品 Protobuf、type support
│   ├── rover_hardware/           # 驱动和 Hardware Endpoint
│   ├── rover_control/            # 运动学、安全、控制算法
│   ├── rover_apps/               # base controller、teleop、UI
│   ├── rover_simulation/         # mock、仿真器、开发工具
│   └── rover_bringup/            # launch、公共运行配置
│
├── products/
│   ├── rover-a/
│   │   ├── product.defconfig
│   │   ├── hardware.yaml
│   │   └── launch.yaml
│   └── rover-b/
│       ├── product.defconfig
│       ├── hardware.yaml
│       └── launch.yaml
│
├── tests/
├── third_party/                  # 可选：厂商 SDK 或补丁
├── openember/                    # 可选：submodule/软链；SDK 模式可无
└── build/                        # 本地构建输出
```

### 5.2 小型产品可使用简化布局

```text
my-product/
├── interfaces/
├── drivers/
├── modules/
├── apps/
├── simulators/
├── products/
└── openember/
```

当某部分需要独立测试、复用、发布或拥有明确依赖时，再提升为 `packages/<name>`。不要仅为了形式拆出大量空包。

### 5.3 包边界

每个产品 package 应尽量具备：

```text
<package>/
├── CMakeLists.txt
├── include/<package>/            # 公共头文件
├── src/
├── proto/                        # 仅 interface package
├── config/                       # 包级默认配置
└── test/
```

第一阶段仍使用标准 CMake，不引入额外 manifest 格式。未来只有在需要跨仓包索引时才增加机器可读 manifest。

## 6. 公开构建契约

### 6.1 产品作为工程根

```cmake
cmake_minimum_required(VERSION 3.20)
project(acme_rover LANGUAGES C CXX)

# SDK 模式
find_package(OpenEmber CONFIG REQUIRED)

# 或源码模式
add_subdirectory(openember)

add_subdirectory(packages/rover_interfaces)
add_subdirectory(packages/rover_hardware)
add_subdirectory(packages/rover_control)
add_subdirectory(packages/rover_apps)
```

OpenEmber 必须逐步消除对子工程不安全的 `CMAKE_SOURCE_DIR` 假设，改用明确的 `OPENEMBER_SOURCE_DIR`、`CMAKE_CURRENT_SOURCE_DIR` 或 target properties。

### 6.2 对外 CMake targets

稳定公开 targets 至少包括：

```text
openember::core
openember::module_api
openember::channel_api
openember::rpc_api
openember::hardware
openember::sensor
openember::actuator
openember::hardware_interface
openember::msgs_cpp
openember::msgs_typesupport
```

产品不得依赖：

- `${OPENEMBER_INCLUDE_DIRS}` 这类全局变量；
- OpenEmber 私有源码目录；
- `services/` 内部实现头文件；
- 未声明为 public 的构建宏和缓存变量；
- 假设 OpenEmber 是顶层 `CMAKE_SOURCE_DIR` 的路径逻辑。

### 6.3 安装与查找

OpenEmber 应提供：

```text
OpenEmberConfig.cmake
OpenEmberConfigVersion.cmake
OpenEmberTargets.cmake
```

并完整安装公开头文件、库、工具、schema、默认配置和 CMake helper。

## 7. Kconfig 与产品型号

### 7.1 一张配置，按功能组织

产品根 Kconfig 是入口：

```text
Acme Rover Configuration - rover-a
├── Build / Toolchain
├── Applications          # 产品应用 + 框架内置应用
├── Communication         # Link / Transport / 消息
├── Hardware              # HAL、Sensor、Actuator、Hardware Interface、产品驱动
├── System Services
├── Libraries / Components
├── Examples / Tests
└── Advanced
```

源码归属与菜单呈现分开：产品扩展仍保存在产品仓，不把配置文件写回 OpenEmber；
但用户按功能找选项，不必在 Framework / Product 两棵树之间切换。

产品根 `Kconfig` 负责 `mainmenu`，并 `source "kconfig/workspace.Kconfig"`。
框架的共享入口不声明 `mainmenu`；CLI 负责绑定框架源码路径和产品扩展文件：

- `kconfig/applications.Kconfig`：插入 Applications；
- `kconfig/hardware.Kconfig`：插入 Hardware；
- 扩展文件可再 source 本仓的模块配置；不存在的扩展使用空片段。

框架的嵌套 source 必须明确指向框架，避免产品同名目录遮蔽。
产品选项仍使用 `PRODUCT_*` 命名，通用转换器将其写入 `config.cmake`。
这个前缀由 Kconfig 管理：生成文件先清除旧的同名前缀 cache，再写入当前配置。
依赖不满足时 Kconfig 可能完全省略布尔选项，产品 CMake 应把未定义视为 OFF，
不要再用 `if(NOT DEFINED ...) set(... ON)` 绕过依赖判断。
型号不是另一个可编辑的菜单选项：用 `--product` 切换，标题显示当前型号，防止
菜单中的型号与构建目录、启动文件各自变化。

### 7.2 Kconfig 与 YAML 的职责

| 配置 | 职责 |
|---|---|
| `product.defconfig` | 编译哪些框架能力、消息、驱动和模块 |
| `hardware.yaml` | 设备实例、总线、地址、参数、驱动选择 |
| `launch.yaml` | 进程/模块拓扑、启动参数、重启策略 |

原则：

- 仅参数不同的型号应共享二进制；
- 只有编译能力、架构或依赖不同才产生不同构建；
- 每个型号使用独立构建目录，避免复用错误配置：

```text
build/rover-a/
build/rover-b/
```

### 7.3 构建入口

```bash
ember menuconfig --product rover-a
ember build
ember run
```

`ember build` 必须：

1. 校验型号名和路径；
2. 首次配置时合并产品根 `openember.defconfig` 与型号 `product.defconfig`，型号优先；已有 `.config` 则保留用户选择；
3. 通过 Kconfig 工具生成完整 `.config`，不得把片段直接当最终配置；
4. 生成 `config.cmake`；
5. 配置和构建对应型号目录；
6. 成功构建后记录配置指纹；版本与工具链锁定仍属于后续 `openember.lock` 工作。

当前命令行为：

- `product.default` 是提交到仓库的默认型号；`build/.ember-active` 是本机最近一次选择，不提交。
- 型号优先级：`--product` > `PRODUCT` 环境变量 > 指定构建目录记录的型号 > 本机最近选择 > `product.default`。
- 默认输出到 `build/<model>/`。`--build-dir`（兼容旧位置参数）可指定其它路径，同型号的后续无参数命令沿用它。显式 `--product` 则回到该型号的默认目录，除非同时指定路径。
- `menuconfig`、`genconfig`、`update`、`build` 使用同一份 `.config`。`build` 每次先解析依赖、生成 `config.cmake` 再配置和编译所有已启用目标。
- `build` / `all` 默认使用可用逻辑 CPU 数的一半（向下取整，至少 1、最多 8）；`-j/--jobs` > `OPENEMBER_JOBS` > 默认值。Cargo 默认跟随，也可用 `CARGO_BUILD_JOBS` 单独覆盖。并行度只属于开发机器，不进入型号配置。
- `run` 使用当前型号目录中的 `bin/launch_manager` 与 `products/<model>/launch.yaml`；`run --dry-run` 只展示命令，不启动节点。
- 配置改动后未重建、最近一次 CMake 配置/构建失败，或 launch_manager 未启用时，`run` 拒绝启动。此指纹不检测任意源码改动，改代码后仍需要主动 `build`。
- `launch.yaml` 是运行拓扑，不由菜单自动改写；关闭其中引用的应用时，要同步调整该型号的 launch。旧构建目录中 CMake 可能保留被关闭目标的二进制，不应凭文件存在判断目标已启用。
- 默认片段更新不会覆盖本地菜单选择；显式 `ember defconfig` 才重置，并备份到 `.config.pre-defconfig`（保留最近一次备份）。
- 一个构建目录不能混用两个型号或两个工程根；旧框架根构建缓存必须使用新目录。旧 `build/.config` 不会被无提示覆盖或自动迁移。

两个 demo 和模板的基础片段默认只启用产品所需节点与 launch_manager，不启用
无关的框架示例和服务。它们仍可在菜单中启用，构建不会用目标过滤偷偷跳过。
旧配置继续保留；若要采用这套新默认，显式执行 `ember defconfig --product <model>`。
若继续启用 web_console/host_monitor，注意当前远端消息包可能缺少核心源码所需的
`openember/msgs/host/v1/host.pb.h`；需要匹配的 openember-msgs 版本或本地源码，
这不是菜单修复能替代的依赖发布工作。

如果 PATH 中的 `ember` 来自旧版独立 `openember-cli`，先使用本仓入口
`./openember/scripts/ember`（在框架仓则为 `./scripts/ember`）。确认后可运行
`./openember/scripts/ember install` 更新用户命令软链和 Bash 补全；这会修改本机
`~/.local/bin/ember` 与 `~/.bashrc`，不是产品仓自动执行的步骤。

回归检查：

```bash
bash scripts/kconfig/test-product-menuconfig.sh ../openember-demo-rover ../openember-demo-agri-station
bash scripts/kconfig/test-workspace-cli.sh
```

前者覆盖框架/模板/demo 的 Kconfig 解析及失败保留原配置；后者使用离线 CMake
消费工程验证型号切换、菜单修改生效、显式重置、自定义目录、补全及错误保护。

## 8. 产品消息与 type support

### 8.1 消息分级

| 层级 | 位置 | 示例 |
|---|---|---|
| 核心公共消息 | `openember-msgs` | Node、Lifecycle、Diagnostics、通用 Sensor/Actuator |
| 产品线消息 | 产品 `*_interfaces` package | ChassisState、StationAlert、MotorFault |
| 跨产品行业消息 | 独立共享仓 | 公司级农业、物流、无人系统消息 |

产品消息不需要先合入 `openember-msgs` 才能使用。

### 8.2 CMake API

```cmake
openember_add_proto_library(
    TARGET acme_rover_msgs
    PROTOS
        acme/rover/v1/chassis.proto
        acme/rover/v1/safety.proto
    IMPORT_DIRS
        ${OPENEMBER_MSGS_PROTO_ROOT}
)
```

产出：

```text
acme::rover_msgs_cpp
acme::rover_msgs_proto
acme::rover_msgs_typesupport
```

### 8.3 Type support

除生成 `.pb.cc/.pb.h` 外，还要生成或注册：

- 完整类型名；
- Protobuf descriptor；
- 创建/销毁消息对象；
- 序列化/反序列化；
- 文本或 JSON 展示；
- schema/version 标识。

这是 `ember topic echo`、录包/回放、代理/桥接、运行时 introspection 和 EmberLite 代码生成处理未知产品消息的基础。

### 8.4 兼容规则

- package 路径包含显式版本，如 `acme.rover.v1`；
- 不复用已使用字段号；
- 删除字段时使用 `reserved`；
- 破坏性变更进入 `v2`；
- 核心消息只接受跨产品通用能力；
- 产品专用字段不为了“方便”污染公共消息。

## 9. Module、App、Pkg 与 Plugin

### 9.1 概念边界

| 概念 | OpenEmber 中的含义 |
|---|---|
| Module | 业务、算法或硬件抽象的逻辑单元 |
| Node/Process | 承载一个或多个 Module 的运行进程 |
| App | 产品拥有 `main()` 的静态组合程序 |
| Pkg | 可被通用 runtime 加载、包含一个或多个 Module 的部署单元 |
| Runtime Plugin | 扩展 transport、日志、存储、执行器等框架能力 |
| Hardware Driver | 实现 Sensor/Actuator/System API 的设备实现 |

业务 Module、Runtime Plugin 和 Hardware Driver 不能混为一类。

### 9.2 Module API

目标接口应保持轻量：

```cpp
class Module {
public:
    virtual ~Module() = default;
    virtual bool Initialize(ModuleContext context) = 0;
    virtual bool Start() = 0;
    virtual void Shutdown() noexcept = 0;
};
```

`ModuleContext` 提供受控的 Node/Channel/RPC、Configurator、Logger、Executor、Timer、Parameters 和 Diagnostics 句柄。Module 不直接依赖 runtime 内部对象。

### 9.3 两种部署模式

第一阶段支持静态 App 模式：

```text
acme_rover_app
  ├── ChassisModule
  ├── SafetyModule
  └── TelemetryModule
```

后续支持 Pkg 模式：

```text
openember_runtime
  ├── librover_base_pkg.so
  └── libtelemetry_pkg.so
```

动态 Pkg 需要稳定 ABI、符号可见性、版本检查和故障隔离。在这些契约完成前，不把所有模块强行动态库化。

### 9.4 Runtime Plugin

Runtime Plugin 仅用于 transport、日志、录包/回放、tracing/metrics、executor、proxy/bridge 等框架级扩展。普通产品业务不应实现为 Runtime Plugin。

## 10. Hardware Interface 扩展模型

### 10.1 分层

```text
Product App / Module
        ↓
OpenEmber Hardware Interface Manager
        ↓
Endpoint / Driver Registry
        ↓
ISystem / ISensor / IActuator / IJointController
        ↓
产品驱动与厂商 SDK
```

底层通信库与 Hardware Component 分离：

```text
drivers/canopen_transport/       CANopen 编解码与通信
drivers/acme_motor/              厂商电机实现
endpoints/rover_chassis/         映射为 OpenEmber 状态/命令接口
```

### 10.2 驱动注册

第一阶段使用编译期 registry：

```cpp
registry.Register(
    "acme.modbus_io_card",
    [](const EndpointConfig& config, const EndpointContext& context) {
        return std::make_unique<ModbusIoEndpoint>(config, context);
    });
```

Hardware Interface 不再以 `if (type == ...)` 硬编码所有外部设备。

### 10.3 配置

```yaml
hardware:
  endpoints:
    - id: field-io-0
      type: environment_sensor
      driver: acme.modbus_io_card
      critical: true
      options:
        endpoint: 192.168.10.20:502
        unit_id: 1
        channels: 10
```

`type` 表达语义接口，`driver` 表达具体实现，`options` 由驱动解释。框架保留公共生命周期、健康状态、超时和诊断字段。

### 10.4 动态驱动

动态 `.so` 驱动是后续能力。启用前必须具备稳定 C ABI 或严格受控 C++ ABI、版本握手、插件发现、符号隔离、加载诊断和安全卸载。第一阶段不要求动态插件。

## 11. 官方 demo 的目标组织

### 11.1 Agri Station

```text
packages/
├── agri_interfaces/             StationStatus、StationAlert
├── agri_fieldbus/               OE-AFP 编解码、寄存器定义
├── agri_hardware/               Modbus IO driver + endpoint
├── agri_station/                阈值、告警、降级业务
├── agri_console/                Qt UI
├── agri_simulation/             IO card mock、sensor mock
└── agri_bringup/                launch 与公共配置
```

### 11.2 Rover

```text
packages/
├── rover_interfaces/            ChassisState、SafetyState、MotorFault
├── rover_hardware/              CAN/RS485 motor、encoder、IMU drivers
├── rover_control/               differential drive、限速、安全
├── rover_apps/                  base controller、teleop
├── rover_simulation/            rover simulator
└── rover_bringup/               launch 与公共配置
```

官方 demo 必须与 `ember new` 生成结构同构，并作为外部消息、外部驱动、多型号构建、核心升级及 source/SDK 两种消费方式的集成测试。

## 12. 版本与升级契约

### 12.1 版本锁定

产品仓记录：

```yaml
openember:
  version: 1.2.0
  source: https://github.com/openember/openember.git
  commit: <commit>

openember_msgs:
  version: 1.2.0

schema_versions:
  hardware: 1
  launch: 1
```

实际格式可由实现阶段确定，但必须可重现且可被 CI 校验。

### 12.2 兼容性承诺

同一 major 版本内优先保证：

- 公共 C/C++ API 源码兼容；
- 已声明稳定的 CMake targets；
- 消息 wire compatibility；
- `hardware.yaml`、`launch.yaml` schema 兼容；
- CLI 主要语义；
- Module/Driver 注册接口。

不承诺私有头文件、全局 CMake 变量、内部目录布局、未声明稳定的 C++ ABI，以及被用户修改的模板源码自动兼容。

### 12.3 核心更新

```bash
ember core status
ember core update 1.3.0
ember doctor
ember build --all-products
```

更新工具负责检查版本约束、更新 lock/submodule/SDK 引用、迁移配置 schema、报告废弃 API/Kconfig，并构建产品矩阵。失败时必须保留原版本和清晰诊断。

### 12.4 模板更新

```bash
ember scaffold diff
ember scaffold doctor
```

工具只提示新目录、CI、构建入口和配置字段；默认不得覆盖产品源码。需要修改时生成 patch 或迁移报告，由用户审核。

## 13. `ember` CLI 目标体验

```bash
# 创建
ember new acme-rover
ember new acme-rover --standalone
ember new acme-rover --sdk /opt/openember/1.2

# 增加能力
ember add interface rover_msgs
ember add module chassis
ember add driver can_motor
ember add app teleop
ember add product rover-b --from rover-a

# 配置、构建、运行
ember menuconfig --product rover-a
ember build --product rover-a
ember build --all-products
ember run --product rover-a
ember doctor

# 更新与交付
ember core update 1.3.0
ember scaffold diff
ember dist --product rover-a
```

生成器以新增文件为主；遇到同名文件必须停止或输出 patch，不静默覆盖。

## 14. 主仓职责

OpenEmber 主仓继续保持开箱即用：

```bash
git clone <openember-url>
cd openember
ember build
```

主仓保留最小示例、API 教程、smoke tests、模板、SDK 安装测试和 product consumer contract tests。

主仓不长期承载完整行业 UI、具体产品 BOM、厂商私有协议、客户业务流程和仅服务单一产品的消息。

通用能力回流遵循：

1. 首先在产品 package 中实现；
2. 第二个产品需要时抽象接口；
3. 稳定后进入独立共享包或 OpenEmber；
4. 通过正式版本替换产品私有实现。

## 15. 当前过渡实现

当前已有：

- rover/agri 从主仓迁出；
- `OPENEMBER_EXTERNAL_APPS_ROOT`；
- `ember new` v0；
- `origin/custom` 路径识别；
- `products/<model>` 草图；
- 产品仓 `apps/` 与 `modules/` 初步分离。

这些能力作为过渡路径保留，但不作为最终公开架构：

```text
cmake -S openember -DOPENEMBER_EXTERNAL_APPS_ROOT=<product>
```

最终迁移到：

```text
cmake -S <product>
  └── find_package(OpenEmber) 或 add_subdirectory(openember)
```

当前仍未完成：

- 产品 Kconfig 真正接入；
- 产品 defconfig 可靠应用；
- 型号构建目录隔离；
- Hardware Interface 外部 factory；
- 产品 proto 统一生成/type-support API；
- OpenEmber 完整 install/export。

## 16. 实施路线

### 16.0 当前落地状态（2026-11-03）

本轮已经完成可运行的 source-consumer 基线：

- OpenEmber 可由产品仓通过 `add_subdirectory(openember)` 消费；
- OpenEmber 内部不再把 `CMAKE_SOURCE_DIR` 误认为框架源码根；
- rover/agri 均以产品仓为 CMake 与 Kconfig 顶层；
- 每个 `products/<model>` 使用独立构建目录和 `product.defconfig`；
- `CONFIG_PRODUCT_*` 可通用映射到产品 CMake cache；
- 产品应用只链接 `openember::*` 和产品 namespaced targets；
- 新增 `openember_add_proto_library()`，两个 demo 均已使用产品自有消息包；
- `ember new` 模板同步采用相同结构。

本轮进一步完成统一的功能菜单，以及 `menuconfig → build → run` 的型号与配置
选择流程（见第 7 节）。产品构建脚本只转发到框架 CLI，不再复制 defconfig
覆盖菜单选择；两个 demo 和新项目模板均使用相同入口。

尚未完成的部分继续按下列 Phase 推进，主要是 installed SDK/export、Hardware
Interface 外部 Registry、Module composition、`openember.lock`、core update 和
standalone/dist。当前外部源码依赖模式已经可用，但不能把它描述成完整的 SDK
发布与自动升级能力。

### Phase A：修正 v0 正确性

- 校验产品名和型号名，阻止路径越界；
- 删除构建脚本中的错误吞噬；
- 每型号独立构建目录；
- 正确应用 defconfig；
- 统一 `ember build` 与产品 build script 行为；
- 为脚手架增加从零构建 CI。

### Phase B：产品根与 SDK 契约

- OpenEmber 支持作为 CMake 子工程；
- 消除关键 `CMAKE_SOURCE_DIR` 假设；
- 完成 install/export；
- 产品改为顶层工程；
- 公布稳定 namespaced targets；
- demo 迁移到新消费方式。

### Phase C：产品 Kconfig 与多型号

- 拆出可嵌入框架 Kconfig；
- 产品 wrapper Kconfig；
- defconfig merge；
- `ember build --product`；
- 型号矩阵和配置指纹；
- schema version 校验。

### Phase D：产品消息与 type support

- `openember_add_proto_library`；
- 产品 descriptor/type registry；
- C++ 与 Nanopb 生成；
- `ember topic echo` 支持产品消息；
- 录包/回放使用统一 type support。

### Phase E：Hardware Interface 外部扩展

- Hardware Interface library target；
- Driver/Endpoint Registry；
- `options` 配置透传；
- 静态产品驱动注册；
- agri/rover 各接入至少一个真实或完整 mock driver；
- 驱动生命周期和诊断测试。

### Phase F：Module composition

- 轻量 `openember::module_api`；
- 静态 App Module Registry；
- 多 Module 单进程；
- 部署配置与逻辑解耦；
- ABI 方案成熟后再支持动态 Pkg。

### Phase G：升级与分发

- `openember.lock`；
- `ember core update`；
- migration/doctor；
- `ember scaffold diff`；
- `ember dist`；
- installed SDK 与 source consumer 双 CI。

## 17. 验收标准

1. `ember new` 生成的新仓可独立构建，不以 OpenEmber 为 CMake 顶层。
2. 同一产品连续构建两个型号不会串用配置。
3. 产品可定义并发布自己的 Protobuf，且 `ember topic echo` 可识别。
4. 产品可注册自己的 Hardware Driver，不修改 OpenEmber 源码。
5. rover/agri demo 均只依赖公开 SDK targets。
6. 产品可从 OpenEmber 一个 minor 版本升级到下一版本并通过 CI。
7. `--standalone`/`dist` 可生成完整可复制工程，并明确其升级边界。
8. OpenEmber 主仓仍可直接 clone、build、run。

## 18. 最终决策摘要

OpenEmber 同时提供两种承诺：

1. **可复制**：用户可以直接把完整工程作为模板使用并任意修改。
2. **可升级**：用户通过独立产品工作区和稳定 SDK 契约持续更新核心。

两种承诺通过不同模式实现，而不是要求同一份被任意修改的源码自动同步上游。

最终推荐路径是：

```text
ROS 2 的工作区与硬件扩展边界
  + AimRT 的 Module/Pkg/Protocol 分层
  + RT-Thread 的 Kconfig、产品配置和 dist 体验
  + FastAPI Template 的一次性脚手架边界
```

这使 OpenEmber 既保持开源框架的独立可维护性，又能成为用户快速搭建、深度定制并长期演进产品的基础。
