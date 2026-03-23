# OpenEmber 构建系统设计文档（Build System Architecture）

- 版本：Draft v1.0
- 适用范围：OpenEmber Framework
- 目标：构建一个可裁剪、可扩展、跨平台、支持插件化通信后端的工业级嵌入式软件框架构建系统
- 技术路线：Kconfig + CMake + FetchContent



## 一、设计目标（Design Goals）

OpenEmber 构建系统必须支持：

| 能力          | 说明                                   |
| ------------- | -------------------------------------- |
| 模块裁剪      | 按需启用 transport / runtime / logging |
| backend 切换  | NNG / ZMQ / Iceoryx                    |
| 依赖模式切换  | system / fetch / vendor                |
| profile 构建  | robot / minimal / desktop              |
| menuconfig UI | 类 Linux kernel                        |
| 离线构建      | 企业环境                               |
| SDK 模式      | 类 ESP-IDF                             |

非目标：

| 不支持              |
| ------------------- |
| 自动包管理器替代    |
| monolithic 单体构建 |



## 二、总体架构（System Architecture）

整体结构：

```bash
User
 ↓
Kconfig (native)
 ↓
menuconfig
 ↓
.config
 ↓
config.h + config.cmake
 ↓
CMake
 ↓
FetchContent / Vendor / System
 ↓
Build Targets
```

即：

```bash
Kconfig → configuration
CMake → build orchestration
FetchContent → dependency provider
```

职责分离非常清晰。

提示：目前使用 Linux kernel 原生 Kconfig 工具链 + CMake，未来有新需求无法实现再考虑使用 kconfiglib（Python）方案



## 三、目录结构设计（Project Layout）

推荐结构：

```bash
OpenEmber/

├── CMakeLists.txt
├── Kconfig
├── configs/
│   ├── minimal.config
│   ├── robot.config
│   └── desktop.config
│
├── cmake/
│   ├── kconfig.cmake
│   ├── fetch.cmake
│   ├── config.cmake
│   └── toolchains/
│
├── scripts/
│   ├── kconfig/
│   │   ├── conf
│   │   ├── menuconfig
│   │   └── nconfig
│   ├── genconfig.py
│   └── menuconfig.py  # 可选 (kconfiglib)
│
├── vendor/
│
├── modules/
│   ├── core/
│   ├── runtime/
│   ├── transport/
│   │   ├── nng/
│   │   ├── zmq/
│   │   └── iceoryx/
│   └── logging/
│
├── include/
│   └── openember_config.h
│
└── build/
```

设计原则：

```
configuration 与 build system 解耦
```



## 四、Kconfig 体系设计

Kconfig 负责：

```
feature selection
dependency graph
profile selection
backend selection
```

示例：

### 顶层 Kconfig

```bash
menu "OpenEmber Configuration"

source "Kconfig.transport"
source "Kconfig.logging"
source "Kconfig.runtime"

endmenu
```



## 五、Transport 配置示例

文件：

```
Kconfig.transport
```

示例：

```bash
menu "Transport Backend"

config OPENEMBER_TRANSPORT_NNG
    bool "Enable NNG backend"
    default y

config OPENEMBER_TRANSPORT_ZMQ
    bool "Enable ZeroMQ backend"

config OPENEMBER_TRANSPORT_ICEORYX
    bool "Enable Iceoryx backend"

endmenu
```



## 六、Logging 模块配置示例

```bash
menu "Logging"

config OPENEMBER_ENABLE_LOGGING
    bool "Enable logging"
    default y

config OPENEMBER_LOG_LEVEL
    int "Default log level"
    default 2

endmenu
```



## 七、Dependency Provider 配置设计

核心能力：

```
System
FetchContent
Vendor
```

Kconfig 示例：

```bash
choice
    prompt "NNG dependency provider"

config OPENEMBER_NNG_USE_SYSTEM
    bool "Use system installed NNG"

config OPENEMBER_NNG_USE_FETCHCONTENT
    bool "FetchContent download NNG"

config OPENEMBER_NNG_USE_VENDOR
    bool "Use bundled NNG"

endchoice
```

这是工业级能力。

类似 ESP-IDF、Zephyr、Buildroot 等项目。



## 八、生成配置文件流程（Configuration Pipeline）

配置流程：

```bash
menuconfig
 ↓
.config
 ↓
genconfig.sh # 或 genconfig.py
 ↓
config.h
config.cmake
 ↓
CMake
```

**生成 config.h**

示例：

```cpp
#define CONFIG_OPENEMBER_TRANSPORT_NNG 1
```

**生成 config.cmake**

示例：

```cmake
set(CONFIG_OPENEMBER_TRANSPORT_NNG ON)
```

用于：

```cmake
if(CONFIG_OPENEMBER_TRANSPORT_NNG)
```



## 九、CMake 构建流程设计

顶层：

```cmake
include(cmake/config.cmake)
```

模块启用：

```cmake
if(CONFIG_OPENEMBER_TRANSPORT_NNG)
    add_subdirectory(modules/transport/nng)
endif()
```



## 十、FetchContent 依赖管理设计

统一封装：

```
cmake/fetch.cmake
```

示例：

```cmake
function(openember_fetch_nng)
    FetchContent_Declare(
        nng
        GIT_REPOSITORY https://github.com/nanomsg/nng.git
        GIT_TAG v1.7.0
    )

    FetchContent_MakeAvailable(nng)
endfunction()
```

调用：

```cmake
openember_fetch_nng()
```



## 十一、Vendor 模式设计

目录：

```
vendor/nng/
```

使用：

```cmake
add_subdirectory(vendor/nng)
```

适用于：

```
offline build
enterprise environment
CI deterministic build
```



## 十二、System 模式设计

示例：

```cmake
find_package(nng REQUIRED)
```

适用于：

```
Linux distro
ROS workspace
apt / yum / pacman
```



## 十三、Profile 配置系统设计（高级能力）

目录：

```
configs/
```

示例：

```
minimal.config
robot.config
desktop.config
```

使用方式：

```
cp configs/robot.config .config
```

然后：

```
menuconfig
```

体验：

≈ Linux kernel



## 十四、menuconfig 入口设计（可选）

建议入口：

```
./scripts/menuconfig.py
```

内部执行：

```
kconfiglib.menuconfig()
```

依赖：

```
kconfiglib
```

安装：

```
pip install kconfiglib
```



## 十五、config.h 自动生成设计（可选）

脚本：

```
scripts/genconfig.py
```

输出：

```
include/openember_config.h
cmake/config.cmake
```

流程：

```
.config
 ↓
parse
 ↓
generate headers
```



## 十六、模块级 CMake 设计规范

例如：

```
modules/transport/nng/CMakeLists.txt
```

示例：

```cmake
add_library(openember_transport_nng)

target_sources(openember_transport_nng
    PRIVATE
    transport_nng.cpp
)
```

统一规范：

```
openember_<module>
```



## 十七、未来 SDK 模式设计（可扩展）

目标：

支持：

```
ember build
```

类似：

```
idf.py
west
```

未来结构：

```
tools/openember-cli
```

命令：

```
ember menuconfig
ember build
ember flash
```

非常适合机器人 SDK。



## 十八、CI 支持设计

推荐：

```
GitHub Actions
```

构建矩阵：

```
minimal
robot
desktop
```

示例：

```
- config: minimal
- config: robot
- config: desktop
```

验证：

```
cross-platform compatibility
```



## 十九、设计优势总结

本方案优点：

| 能力          | 支持 |
| ------------- | ---- |
| 模块裁剪      | ✅    |
| backend 切换  | ✅    |
| 离线构建      | ✅    |
| profile 构建  | ✅    |
| menuconfig UI | ✅    |
| SDK 扩展      | ✅    |
| 企业部署      | ✅    |

成熟度：

≈ Zephyr 构建体系等级