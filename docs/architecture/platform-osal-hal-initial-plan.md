# Platform OSAL / HAL 初版实施规划

本文档基于 [`hal.md`](./hal.md) 设计规范，给出**规范评审摘要**、**与当前仓库的差异说明**，以及 **OSAL + HAL 初版（v0）** 的范围、目录、API 轮廓与落地步骤。

---

## 一、对《HAL 设计规范》的评审

### 1.1 优点（建议保持）

- **职责边界清晰**：HAL 只做硬件访问抽象，协议、仲裁、业务上移，符合可维护分层。
- **C ABI 优先 + 可选 C++**：满足插件、动态加载与多语言绑定，与 OpenEmber 长期演进一致。
- **统一命名 `oe_<device>_<action>()`**：避免平台前缀泄漏，利于跨 SoC/跨 OS。
- **能力查询与零拷贝**：对视觉/Agent 场景有前置设计，避免后期 API 大改。
- **与 OSAL 分离**：线程/锁/定时器不进 HAL，避免「硬件抽象」与「OS 抽象」混淆。

### 2.2 待补全或需在实现中落地的点

| 规范中的表述 | 建议 |
| ------------ | ---- |
| HAL 与 `platform` 下的 `board` / `arch` | 初版可只建目录与 README，**不实现**板级描述与 ISA 选择逻辑。 |
| Multimedia 强依赖 `dma-buf` / zero-copy | v0 可只定义 **capability 结构体 + 空实现或 TODO**，避免第一版就绑 V4L2/GStreamer。 |
| 与现有 `components/HAL` | 当前为 **演示用可执行文件**（`main.c` 打印 hello），与规范中的 **platform HAL** 不同；初版应在文档与 CMake 中 **显式区分命名**（见下文），后续可迁移或重命名。 |
| 错误码与资源生命周期 | 规范未统一 `oe_err_t`、句柄关闭顺序；初版需定义 **最小错误类型 + `open/close` 语义**。 |

### 2.3 风险与约束（初版需接受）

- **仅 Linux 用户态**：初版以 `pthread`、标准文件描述符、`poll`/`epoll` 为主；RTOS 留扩展点即可。
- **仲裁与多进程**：规范要求 DeviceManager 仲裁；OSAL/HAL v0 **不实现**锁文件或 IPC 仲裁，仅保证 API 可单进程/单线程安全使用。
- **稳定性**：v0 标为 **experimental**，允许在 `platform/osal`/`platform/hal` 内小步迭代，但 **C 头文件** 尽量保持前向兼容。

---

## 二、初版目标（v0）

### 2.1 OSAL（`platform/osal`）

**目标**：提供与业务无关的 OS 抽象，供 HAL、core、modules 使用。

**v0 范围（建议）**：

| 类别 | 能力 | Linux 实现要点 |
| ---- | ---- | ---------------- |
| 类型/错误 | `oe_result_t`、`OE_OK`、错误码枚举 | 头文件即可 |
| 线程 | `oe_thread_create` / `join` / `detach` | `pthread` |
| 互斥 | `oe_mutex_init/lock/unlock/destroy` | `pthread_mutex_t` |
| 条件变量（可选） | `oe_cond_*` | `pthread_cond_t` |
| 时间 | `oe_sleep_ms`、`oe_monotonic_ns` | `clock_gettime`、 `nanosleep` |
| 定时器（软定时） | 单次/周期回调，基于线程 + `oe_sleep` 或 `timerfd`（二选一） | v0 可用 **简单线程 + sleep** 降低复杂度 |

**v0 明确不包含**：`socket`/`ipc`/`shm` 全量实现（可作为 v1 条目，或仅保留头文件占位）。

### 2.2 HAL（`platform/hal`）

**目标**：提供 **C ABI** 的硬件访问入口；v0 只实现 **少量、高复用、易在 PC 上验证** 的类。

**v0 范围（建议）**：

| 子域 | v0 内容 | 说明 |
| ---- | ------- | ---- |
| `hal/bus` | **UART（串口）** | `termios` + `read/write`/非阻塞 + `poll`；命名 `oe_uart_*`。 |
| `hal/device` | **GPIO（sysfs 或 chardev）** | 优先 **Linux sysfs**（`/sys/class/gpio`）或 `libgpiod` 二选一；v0 文档中写清依赖，实现选一种。 |
| 其它 bus（SPI/I2C/CAN） | **仅头文件 stub** | 返回 `OE_ERR_NOT_SUPPORTED` 或宏开关关闭编译，避免空壳误导。 |
| `multimedia` / `storage` | **不实现** | 仅 `include/openember/hal/capabilities.h` 中定义 **capability 结构体占位**（与规范对齐，无运行时）。 |

**能力查询（v0）**：

- UART：波特率、数据位、校验（`oe_uart_config_t` + `oe_uart_get_caps` 可简化为「返回当前配置」）。
- GPIO：输入/输出、有效电平（若 sysfs 能力有限，文档说明）。

### 2.3 与规范文档的对应关系

- 命名：`oe_<module>_<verb>()`（与 `hal.md` 第十四章一致）。
- 语言：对外 **C**；内部实现 `.c`；可选后续增加 `platform/hal/cpp/*.hpp`。

---

## 三、推荐目录结构（初版）

```bash
platform/
├── CMakeLists.txt              # add_subdirectory(osal) + add_subdirectory(hal)
├── README.md                   # platform 层说明与版本状态
├── osal/
│   ├── CMakeLists.txt
│   ├── include/openember/osal/
│   │   ├── types.h             # oe_result_t, OE_* 错误码
│   │   ├── thread.h
│   │   ├── mutex.h
│   │   └── time.h
│   └── src/linux/
│       ├── oe_thread.c
│       ├── oe_mutex.c
│       └── oe_time.c
└── hal/
    ├── CMakeLists.txt
    ├── include/openember/hal/
    │   ├── types.h             # 与 osal 类型兼容或 include osal/types.h
    │   ├── uart.h
    │   ├── gpio.h
    │   └── capabilities.h      # 预留能力描述（camera 等）
    └── src/linux/
        ├── oe_uart_linux.c
        └── oe_gpio_linux.c
```

**与规范中 `hal.md` 的 `platform/hal/bus|device|...` 对齐方式**：在 `include/openember/hal/` 下用 **子目录或文件名前缀** 区分 bus/device（例如 `uart.h` 属于 bus），`src/linux` 下按实现拆分。

---

## 四、构建与集成（初版）

1. **根 `CMakeLists.txt`**：在 `components`/`modules` 之后、`apps` 之前（或紧接 `components`）增加 `add_subdirectory(platform)`，保证：
   - `openember_osal` 为 **静态库**（或 `INTERFACE` 仅头文件阶段，不推荐长期）。
   - `openember_hal` 依赖 `openember_osal`（HAL 内部可能用到 mutex/time）。
2. **对外头文件路径**：`include_directories` 或 `target_include_directories` 暴露 `platform/osal/include`、`platform/hal/include`。
3. **现有 `components/HAL`**：保留现状或改名为 `components/hal_demo`，避免与 `platform/hal` 混淆（**独立提交**）。

---

## 五、测试策略（初版）

| 层级 | 测试 |
| ---- | ---- |
| OSAL | 单元测试：mutex 互斥、线程 join、时间单调性（`test/` 下新增 `test_osal_*.c`）。 |
| HAL | UART：环回或虚拟串口（`socat`）；GPIO：需硬件或 mock（v0 可 **仅编译测试** + 文档说明）。 |

---

## 六、实施阶段（建议）

| 阶段 | 内容 | 产出 |
| ---- | ---- | ---- |
| **P0** | OSAL：types + mutex + time + thread | 可链接静态库 + 最小单元测试 |
| **P1** | HAL：UART Linux 实现 + 头文件 API 冻结 | 示例程序或 `examples` 下的最小 demo |
| **P2** | HAL：GPIO（一种后端）+ 错误码与日志统一 | 与 `DeviceManager` 对接设计文档 |
| **P3** | capability 扩展、C++ wrapper、Kconfig 开关 | 与 `hal.md` 后几章对齐 |

---

## 七、小结

- **初版价值**：在 **不触碰业务模块** 的前提下，把 **OSAL 与 HAL 的边界、命名、目录** 固化到代码树，并可在 Linux 上 **编译 + 部分单元测试**。
- **与 `hal.md` 的关系**：本规划是 **v0 裁剪实现**；多媒体零拷贝、storage 等按规范 **预留接口**，避免一次性实现过重。

后续若你确认，可按 **P0 → P1** 顺序提交多个小 PR，并在 `Kconfig` / `platform/Kconfig` 中增加 `OPENEMBER_ENABLE_PLATFORM_HAL` 等开关（可选）。
