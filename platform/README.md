# Platform 层

- **`osal/`**：操作系统抽象（线程、互斥、时间等），初版基于 Linux `pthread`。
- **`hal/`**：（后续）硬件抽象，见 `docs/architecture/hal.md`。

构建开关：`OPENEMBER_ENABLE_OSAL`（默认开启，可由 Kconfig / `config.cmake` 覆盖）。
