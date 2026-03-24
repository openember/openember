# OpenEmber 框架分层设计文档（Layered Architecture Design Guide）

本文档是一份正式的 OpenEmber 框架分层设计文档（可作为架构规范 v1.0），内容结构是按长期可维护的 middleware 框架级标准写的，可以直接作为：

- 项目架构设计说明
- 开源仓库 /docs/architecture.md
- 软件著作权技术文档材料
- 团队协作规范
- 模块开发指南基础版本

---

- 版本：v1.0
- 目标读者：OpenEmber 核心开发者 / 插件开发者 / 第三方模块开发者
- 适用范围：OpenEmber 构建系统（CMake + Kconfig + FetchContent）整体架构设计



## 一、设计目标

OpenEmber 的分层结构设计目标包括：

- 构建可扩展的软件框架体系
- 支持插件生态
- 支持第三方库灵活接入
- 支持嵌入式 Linux 场景
- 支持未来跨平台能力
- 支持用户扩展模块
- 支持 Kconfig 配置体系
- 支持离线构建 / 企业构建
- 支持 middleware 级别演进

本设计参考如下成熟框架思想：

- Linux kernel
- Zephyr RTOS
- ROS2 middleware
- ESP-IDF

目标是构建一个可以长期演进的机器人软件基础设施框架。

## 二、总体分层结构

OpenEmber 推荐采用五层结构模型：

1. Application Layer
2. Module Layer
3. Component Layer
4. Core Layer
5. Platform Layer

目录结构如下：

```bash
openember/

├── apps/
├── modules/
├── components/
├── core/
├── platform/
├── third_party/
├── vendor/
├── configs/
└── tools/
```

其中前五层构成框架主体结构，其余目录为工程辅助结构。

## 三、整体层级关系示意

依赖方向如下：

```bash
apps
↓
modules
↓
components
↓
core
↓
platform
```

说明：

- 上层可以依赖下层（可以跨层依赖）
- 下层不能依赖上层

该规则保证架构稳定性。

## 四、Platform Layer（平台抽象层）

目录：`platform/`

职责：提供操作系统接口抽象 + 硬件接口抽象。

目标：避免上层直接访问 Linux API 或设备驱动接口。

典型结构：

```bash
openember/

platform/
 ├── osal/
 │   ├── thread
 │   ├── mutex
 │   ├── timer
 │   ├── socket
 │   └── shm
 │
 ├── hal/
 │   ├── bus/
 │   │   ├── uart
 │   │   ├── spi
 │   │   ├── can
 │   │   └── i2c
 │   │
 │   ├── device/
 │   │   ├── gpio
 │   │   ├── pwm
 │   │   ├── adc
 │   │   └── watchdog
 │   │
 │   └── multimedia/
 │       ├── camera
 │       ├── audio
 │       └── display
 │
 ├── board/
 │   ├── rk3588
 │   ├── jetson
 │   └── qemu
 │
 └── arch/
     ├── arm64
     └── x86_64
```

示例说明：

- thread 封装 pthread
- mutex 封装 pthread_mutex
- socket 封装 POSIX socket

hal 示例：

- HalUart
- HalSpi
- HalCan
- HalI2c

设计原则：platform 层负责屏蔽系统差异。

os 未来支持：

- Linux
- RTOS
- Windows
- macOS

无需修改上层代码。

## 五、Core Layer（框架核心层）

目录：`core/`

职责：提供 middleware 运行核心机制。

典型模块：

```bash
core/

├── runtime/
├── executor/
├── node/
├── topic/
├── parameter/
├── service/
└── lifecycle/
```

示例能力：

- Node 生命周期管理
- 发布订阅系统
- 参数系统
- 任务调度器
- 执行器模型

典型示例：

```c++
class Node
class Executor
class TopicManager
```

设计原则：core 层必须保持稳定。

禁止放入：

- 设备驱动
- 协议实现
- 算法模块
- 数据库实现

否则会导致核心层膨胀。

## 六、Component Layer（基础组件层）

目录：`components/`

职责：提供基础可复用能力组件。

特点：

- 通用
- 稳定
- 跨模块复用
- 不依赖具体业务

典型模块示例：

```bash
components/

├── logging/
├── serialization/
├── container/
├── memory/
├── database/
├── ipc/
├── transport/
└── config/
```

典型组件示例：

```bash
Logger
RingBuffer
LockFreeQueue
Serializer
SqliteWrapper
SharedMemoryChannel
```

判断标准：

- 是否被多个模块复用？
- 是否属于基础设施能力？
- 是否不依赖具体设备？

如果满足上述条件，则属于 components。

## 七、Module Layer（功能模块层）

目录：`modules/`

职责：提供插件式功能模块集合。

特点：

- 可选启用
- 支持 Kconfig 控制
- 允许依赖第三方库
- 允许依赖硬件
- 允许依赖协议

典型模块分类：

```bash
modules/

├── transport/
│   ├── nng/
│   ├── zmq/
│   └── lcm/
│
├── drivers/
│   ├── imu/
│   ├── lidar/
│   └── motor/
│
├── dds/
│   └── fastdds/
│
├── algorithms/
│
├── perception/
│
└── navigation/
```

示例说明：

- transport/nng
  用于实现通信后端插件
- transport/zmq
  用于实现通信后端插件
- imu driver
  用于读取传感器数据

这些模块均属于插件层。

设计原则：modules 是生态扩展层，允许用户新增模块。

例如：

```bash
external_modules/
my_robot_driver/
```



## 八、Application Layer（应用层）

目录：`apps/`

职责：提供应用程序入口节点。

典型应用示例：

```bash
apps/

├── launch/
│
├── node_camera/
│
├── node_controller/
│
├── node_logger/
│
└── node_web/
```

典型程序类型：

- 节点程序
- 系统启动程序
- 调试工具
- 测试程序

示例：

- openember_launch
  负责启动多个节点

设计原则：apps 层不属于框架核心，属于最终运行程序集合。

## 九、Third Party Layer（第三方依赖层）

目录：`third_party/`

职责：存放第三方源码。

典型示例：

```bash
third_party/

├── nng/
│
├── zmq/
│
├── sqlite/
│
└── flatbuffers/
```

说明：通常由 FetchContent 下载，不建议修改源码。

## 十、Vendor Layer（内置依赖层）

目录：`vendor/`

职责：提供离线构建能力。

典型场景：

- 企业环境
- 内网环境
- 安全环境

示例：

```bash
vendor/

├── nng/
│
└── sqlite/
```

说明：与 third_party 的区别：

- third_party 可动态下载
- vendor 必须本地存在（如果你需要修改某个第三方库的代码，建议放在 vendor 目录）

## 十一、Configs Layer（配置模板层）

目录：`configs/`

职责：提供默认配置模板。

示例：

```bash
configs/

├── minimal.config
├── robot.config
└── desktop.config
```

类似 Linux kernel defconfig。

## 十二、Tools Layer（工具层）

目录：`tools/`

职责：提供辅助开发工具。

典型工具示例：

- 代码生成工具
- 日志分析工具
- 调试工具
- CLI 工具

示例：

- openember-cli

## 十三、HAL 与 OS 抽象的关系

推荐结构：

```bash
platform/
├── os/
└── hal/
```

关系如下：

```bash
应用
↓
模块
↓
组件
↓
core
↓
hal
↓
os
↓
驱动
```

说明：

- HAL 屏蔽硬件接口差异
- OS 屏蔽系统接口差异

## 十四、通信后端架构设计示例

推荐结构：

```bash
third_party/
├── nng/
├── zmq/
└── lcm/

modules/
├── transport/
│   ├── nng/
│   ├── zmq/
│   └── lcm/
```

说明：

- third_party 存放源码
- modules 存放适配层

## 十五、数据库组件设计示例

推荐结构：

```bash
third_party/
└── sqlite/

components/
└── database/
    └── sqlite/
```

说明：

- sqlite 为第三方源码
- components/database 提供统一接口封装

## 十六、硬件接口分层设计示例

推荐三层结构：

1. platform/hal
2. components/transport
3. modules/drivers

关系如下：

```bash
设备驱动
↓
transport 抽象
↓
hal 抽象
↓
Linux driver
```

示例：

```bash
UART
CAN
SPI
I2C
Bluetooth
WiFi
```



## 十七、组件与模块边界判断规则

判断是否属于 components：

- 是否为基础设施能力
- 是否跨模块复用
- 是否不依赖具体设备

判断是否属于 modules：

- 是否插件能力
- 是否可裁剪
- 是否依赖协议
- 是否依赖设备

示例：

```bash
RingBuffer → components
Logger → components
NNG backend → modules
IMU driver → modules
```



## 十八、分层设计优势总结

该架构具备以下优势：

- 支持插件生态
- 支持跨平台能力
- 支持企业离线构建
- 支持 middleware 架构演进
- 支持用户扩展模块
- 支持 Kconfig 配置系统
- 支持第三方库灵活接入
- 支持长期维护

适用于：

- 机器人系统
- 嵌入式 Linux 系统
- 边缘计算系统
- 分布式控制系统
- 智能设备系统

## 十九、未来扩展能力规划

未来可支持 `external_modules/` 或 `packages` 用于用户扩展插件

支持方式：

```cmake
add_subdirectory()
```

或环境变量注册路径

类似 Zephyr 外部模块机制。

## 二十、总结

OpenEmber 推荐采用如下正式结构：

```bash
apps/
modules/
components/
core/
platform/
third_party/
vendor/
configs/
tools/
```

该结构适用于构建长期可维护的软件框架体系。

同时支持 middleware 级生态扩展能力。