# LPIO (Linux Peripheral I/O)

各设备类实现使用 RAII + Builder 模式

## 目录结构

```
lpio/
├── include/lpio/
│   ├── DeviceBase.hpp       ← 统一异常体系与设备基类
│   ├── SerialPort.hpp
│   ├── RS485.hpp
│   ├── SBUSReader.hpp
│   ├── I2CBus.hpp
│   ├── SPIBus.hpp
│   ├── CANBus.hpp
│   ├── GPIO.hpp
│   ├── PWM.hpp
│   ├── OneWire.hpp
│   └── private/             ← 内部实现支持，不作为稳定 API
│       ├── UniqueFd.hpp
│       └── FdDeviceBase.hpp
├── src/
│   ├── SerialPort.cpp
│   ├── RS485.cpp
│   ├── SBUSReader.cpp
│   ├── I2CBus.cpp
│   ├── SPIBus.cpp
│   ├── CANBus.cpp
│   ├── GPIO.cpp
│   ├── PWM.cpp
│   └── OneWire.cpp
├── tests/
│   ├── mock/              ← MockDevice for unit testing
│   └── integration/       ← 需要真实硬件的测试
├── examples/
│   ├── serial_echo.cpp
│   ├── i2c_scan.cpp
│   └── can_logger.cpp
├── cmake/
│   └── FindLibgpiod.cmake
└── CMakeLists.txt
```


## 几个关键设计决策

错误处理策略 — 推荐两档：

- C++17：抛出自定义 DeviceError（携带 errno + 路径 + 操作名）
- C++23：返回 std::expected<T, DeviceError>，对实时线程更友好（暂不实现）

**Mock 层** — 接口继承的最大好处：单元测试时用 MockSerialPort 替换真实设备，完全不改上层代码。

**异步支持** — 初期用同步阻塞 + select/poll 超时参数，后期可无缝接入 io_uring 或 asio。

**依赖** — 尽量只依赖 Linux 内核头文件（<termios.h>、<linux/spi/spidev.h>、<linux/can.h>）。GPIO 当前使用 Linux GPIO chardev v1 ioctl（<linux/gpio.h>），避免 sysfs GPIO。

**资源管理** — 文件描述符通过内部 `private/UniqueFd.hpp` 转移所有权；单 fd 设备继承内部 `detail::FdDeviceBase`，多资源或非 fd 设备按自身语义实现 RAII。设备对象不可拷贝、可移动，析构与失败路径都会自动释放已获取资源。

**设备关系** — `SerialPort` / `I2CBus` / `SPIBus` / `CANBus` 是单 fd 设备；`RS485` 继承 `SerialPort`；`SBUSReader` 组合 `SerialPort`，只暴露 SBUS 帧读取语义；`GPIO` 管理 chip/line/event fd；`PWM` 管理 sysfs export/unexport 状态；`OneWire` 通过 kernel w1/sysfs 读取设备数据。
