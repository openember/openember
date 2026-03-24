# OpenEmber HAL 设计规范（Architecture Spec）



## 一、设计目标

OpenEmber HAL（Hardware Abstraction Layer）的目标是：

- 统一硬件访问接口
- 屏蔽 Linux 驱动差异
- 支持跨平台迁移能力
- 支持多进程设备访问架构
- 支持 DeviceManager
- 支持 AI Agent Runtime
- 支持 C / C++

HAL 是 OpenEmber platform 层的重要组成部分，但不是 platform 的全部。

结构关系如下：

```bash
platform
 ├── osal
 ├── hal
 ├── board
 └── arch
```



## 二、HAL 的职责范围

HAL 负责：统一硬件访问接口抽象

HAL 不负责：

- 协议解析
- 业务逻辑
- 设备访问仲裁
- 缓存策略
- 任务调度

职责边界如下：

| 层               | 职责         |
| ---------------- | ------------ |
| apps             | 应用逻辑     |
| modules/device   | 设备功能封装 |
| modules/protocol | 协议解析     |
| device_manager   | 设备访问仲裁 |
| HAL              | 硬件接口抽象 |
| driver           | 内核驱动     |

关系如下：

```bash
apps
 ↓
modules/device
 ↓
modules/protocol
 ↓
HAL
 ↓
driver
```



## 三、HAL 在 OpenEmber 架构中的位置

完整架构如下：

```bash
applications
 ↓
services（DeviceManager optional）
 ↓
modules/device
 ↓
modules/protocol
 ↓
platform/hal
 ↓
kernel driver
```

HAL 位于：用户空间硬件访问最底层



## 四、HAL 分层设计

HAL 推荐目录结构如下：

```bash
platform/hal/

├── bus/
├── device/
├── multimedia/
└── storage/
```

说明如下：



### Bus HAL（通信总线接口层）

目录：`platform/hal/bus/`

提供基础通信接口抽象：

```bash
uart
spi
i2c
can
usb
ethernet
```

接口示例：

```bash
oe_uart_open()
oe_uart_read()
oe_uart_write()

oe_can_open()
oe_can_send()
oe_can_recv()
```

职责：统一设备通信接口访问方式

不负责：

```bash
CANopen
Modbus
UAVCAN
```

这些属于 `modules/protocol`



### Device HAL（基础设备接口层）

目录：`platform/hal/device/`

包括：

```bash
gpio
pwm
adc
rtc
watchdog
```

接口示例：

```c++
oe_gpio_set()
oe_pwm_start()
oe_adc_read()
```

职责：统一基础设备访问接口



### Multimedia HAL（多媒体接口层）

目录：`platform/hal/multimedia/`

包括：

```bash
camera
audio
display
codec
```

接口示例：

```c++
oe_camera_open()
oe_camera_start()
oe_camera_get_frame()
```

必须支持：

```bash
zero-copy
shared memory
dma-buf
```

否则无法支持：

- 视觉系统
- AI系统
- 机器人系统



### Storage HAL（存储接口层）

目录：`platform/hal/storage/`

包括：

```
block
flash
eeprom
file
```

接口示例：

```c++
oe_block_read()
oe_flash_write()
```

职责：统一底层存储访问接口



## 五、HAL 设计原则

HAL 设计必须遵循以下原则：

### 原则 1：HAL 只做接口抽象

禁止：

- 协议解析
- 业务逻辑
- 调度逻辑
- 缓存管理

示例：

错误设计：

```c++
hal_can_send_canopen_packet()
```

正确设计：

```c++
hal_can_send_frame()
```



### 原则 2：HAL 不负责设备访问仲裁

HAL 不处理：

- 多进程访问控制
- 设备占用控制
- 权限控制

这些属于 DeviceManager



### 原则 3：HAL 必须支持能力查询接口

示例：

```c++
oe_camera_query_caps()
```

返回：

```
resolution
fps
pixel_format
```

用途：

- 设备自动适配
- 动态能力发现
- Agent Runtime tool registry



### 原则 4：HAL 必须支持零拷贝能力

示例：

```c++
oe_camera_export_dmabuf()
```

适用于：

```
video
lidar
radar
audio
```



### 原则 5：HAL API 必须稳定

HAL 是长期稳定 ABI

因为 modules、apps、agent runtime 全部依赖 HAL。



## 六、HAL 与 OSAL 的边界

OSAL 位于：`platform/osal`

提供：

```
thread
mutex
timer
ipc
socket
clock
shared memory
```

示例：

```c++
oe_thread_create()
oe_mutex_lock()
oe_timer_start()
```

HAL 不包含：

```
thread
mutex
timer
```

这些属于 OS abstraction，而不是 hardware abstraction。



## 七、HAL 与 modules/device 的关系

关系如下：

```
modules/device
 ↓
HAL
```

示例：

```
modules/device/camera_uvc
 ↓
hal_camera_open()
```

职责区别：

| 层            | 职责     |
| ------------- | -------- |
| HAL           | 访问设备 |
| device module | 使用设备 |

示例：

HAL：

```c++
oe_camera_get_frame()
```

device module：

```c++
camera_start_stream()
```



## 八、HAL 与 modules/protocol 的关系

协议层位于：`modules/protocol`

例如：

```
modbus
canopen
uavcan
j1939
mavlink
```

关系如下：

```
modules/protocol
 ↓
HAL bus interface
```

示例：

```
canopen
 ↓
hal_can_send_frame()
```



## 九、HAL API 语言设计规范

HAL 必须提供：C ABI 接口

原因：

- 跨编译器稳定
- 支持插件系统
- 支持动态加载
- 支持多语言绑定

例如：

```
oe_uart_open()
oe_uart_write()
```



## 十、C++ Wrapper 设计规范

在 C ABI 之上提供：C++ wrapper

示例：

```c++
namespace oe {

class UART {

public:

    UART(const std::string& dev);
    
    size_t write(const void* buf, size_t size);

};

}
```

内部调用：

```bash
C HAL API
```

结构：

```bash
C++ wrapper
 ↓
C ABI
 ↓
driver
```



## 十一、HAL 推荐目录结构

建议如下：

```bash
platform/hal/

include/
 └── openember/
     └── hal/
         ├── uart.h
         ├── spi.h
         ├── can.h
         ├── gpio.h
         └── camera.h

cpp/
 └── uart.hpp

src/
 ├── uart_linux.c
 ├── can_socketcan.c
 └── camera_v4l2.c
```

说明：

| 文件 | 类型        |
| ---- | ----------- |
| .h   | C API       |
| .hpp | C++ wrapper |
| .c   | 平台实现    |



## 十二、HAL 与 DeviceManager 的关系

关系如下：

```
DeviceManager
 ↓
modules/device
 ↓
HAL
```

DeviceManager 负责：

- 设备注册
- 设备占用控制
- 设备状态管理
- 访问仲裁

HAL 负责：

- 设备访问接口

职责完全不同。



## 十三、HAL 与 AI Agent Runtime 的关系

未来 Agent Runtime 依赖：

```
HAL capability discovery
```

例如：

```c++
oe_camera_query_caps()
```

用于：自动注册 agent tools

示例：

```c++
camera.capture
camera.start_stream
camera.set_resolution
```

成为 Agent Tool Registry 数据源



## 十四、HAL API 命名规范

推荐统一格式：

```
oe_<device>_<action>()
```

示例：

```c++
oe_uart_open()
oe_uart_write()

oe_can_send()

oe_camera_start()
```

禁止：

```c++
linux_uart_open()
rk_uart_open()
```

避免平台泄漏。



## 十五、最终 HAL 分层总结

完整结构如下：

```
applications
 ↓
modules/device
 ↓
modules/protocol
 ↓
platform/hal
 ↓
kernel driver
```

HAL 是 OpenEmber runtime 最底层用户态硬件接口抽象层

它同时是未来 DeviceManager、Robot Runtime、AI Agent Runtime、Embodied Runtime 的基础能力核心层。
