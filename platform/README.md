# Platform 层

- **`osal/`**：操作系统抽象（线程、互斥、时间等），初版基于 Linux `pthread`。
- **`hal/`**：硬件抽象（C ABI），初版含 **storage/file**（POSIX `fopen` 等）与 **bus/uart**（`termios`），见 `docs/architecture/hal.md`。

构建开关：`OPENEMBER_ENABLE_OSAL`、`OPENEMBER_ENABLE_HAL`（默认开启；HAL 依赖 OSAL，可由 Kconfig / `config.cmake` 覆盖）。
