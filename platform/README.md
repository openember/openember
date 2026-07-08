# Platform 层

- **`osal/`**：操作系统抽象（线程、互斥、时间等），初版基于 Linux `pthread`。
- **`lpio/`**：Linux 外设 I/O（C++ RAII），覆盖串口、RS485、SBUSReader、I2C、SPI、CAN、GPIO、PWM、1-Wire 等。

构建开关：`OPENEMBER_ENABLE_OSAL`、`OPENEMBER_ENABLE_LPIO`（默认开启，可由 Kconfig / `config.cmake` 覆盖）。
