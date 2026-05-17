# LPIO (Linux Peripheral I/O)

各设备类实现使用 RAII + Builder 模式

## 目录结构

```
lpio/
├── include/lpio/
│   ├── DeviceBase.hpp
│   ├── SerialPort.hpp
│   ├── I2CBus.hpp
│   ├── SPIBus.hpp
│   ├── CANBus.hpp
│   ├── GPIO.hpp
│   └── Error.hpp          ← 统一异常体系
├── src/
│   ├── SerialPort.cpp
│   ├── I2CBus.cpp
│   ├── SPIBus.cpp
│   ├── CANBus.cpp
│   └── GPIO.cpp
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

**依赖** — 尽量只依赖 Linux 内核头文件（<termios.h>、<linux/spi/spidev.h>、<linux/can.h>），GPIO 可选依赖 libgpiod（比 sysfs 更现代）。