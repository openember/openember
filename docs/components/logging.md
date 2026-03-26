# OpenEmber 日志模块设计（Logging）

- **版本**：v0.1（草案）
- **目标读者**：框架开发者、模块/应用开发者、运维
- **关联**：分层模型见 [layered-architecture-design-guide.md](../architecture/layered-architecture-design-guide.md)；语言策略见 [language-policy.md](../language-policy.md)。

---

## 1. 结论摘要（先回答常见问题）

### 1.1 同时兼容 zlog 与 spdlog，还是只选一个？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **双后端 + 统一门面** | 可按场景切换（纯 C 链路用 zlog，C++ 用 spdlog） | 维护两套适配、测试矩阵变大、行为细节（格式、轮转）需对齐 |
| **单一后端 + 全仓统一** | 实现与排障简单、依赖少 | 若强需求「仅 C 依赖」则需选 C 库或自写薄封装 |
| **自研核心 + 可插拔 sink** | 完全可控 | 成本高，轮转/多进程/性能都要自己扛 |

**建议（OpenEmber 现状：C++ 主线 + Platform 大量 C）**：

1. **对外只维护一套「OpenEmber 日志语义」**（级别、字段、模块名约定），**实现上优先单一后端**，避免长期双轨。
2. **默认后端推荐 `spdlog`（C++）**：与主线一致、生态成熟、异步/轮转/多 logger 开箱即用。
3. **纯 C 代码路径（OSAL/HAL/第三方 C 库）**：不直接链接 spdlog，而是通过 **稳定的 C ABI**（见 §4）调用；该 ABI 在进程内由 **同一后端实现**（例如 spdlog 适配层）完成，**不必**为 C 再单独跑一套 zlog，除非有 **强约束**（例如仅允许静态链接某 C 日志库、许可证、体积）。
4. **zlog 的定位**：可作为 **备选后端**（Kconfig 切换）或 **历史/嵌入式 Linux 场景**下的替代，但应在文档中写明「与默认后端二选一」，而不是让业务代码同时依赖两套 API。

**不推荐**：在业务里混用 `zlog_*` 与 `spdlog::` 两套原生 API；应统一到 `LOG_I` / `OE_LOG_INFO` 等门面。

### 1.2 zlog / spdlog 是库还是服务？

- 二者本质都是 **链接进进程的用户态库**，**不是**必须独立运行的「日志服务进程」。
- **多进程各自链接库**：每个进程自己 `init`，各自写文件或 stdout；**默认不会自动合并到同一文件**。
- **若要「多进程写到一块」**，需要额外机制，例如：
  - **syslog / journald**（系统统一收集）；
  - **同一日志文件 + 单写者**：由 **一个日志守护进程**（Unix 域套接字 / shm ring）集中写盘（可选）；
  - **应用层聚合**：各进程写独立文件，由运维用 **logrotate + rsyslog / Vector / Fluent Bit** 汇总。

**不要求** OpenEmber 第一版就实现独立 logger 守护进程；**推荐**先「每进程初始化 + 文件/控制台 + 可选 syslog」，后续再按需加集中式。

### 1.3 线程安全与实时性

- **spdlog**：logger/sink 设计为 **多线程安全**（同步 sink）；**异步模式**（`async_logger`）可把格式化与 I/O 挪到后台线程，降低业务线程抖动，适合有一定吞吐的实时任务，但仍需注意极端负载下的队列背压策略。
- **zlog**：多线程场景需按官方说明使用（通常按 category + 锁），具体以后端为准。
- **硬实时（RTOS/确定性线程）**：日志应 **降级、批量、或专用无锁 ring**；不在本通用方案中展开，仅预留「可关闭 / 仅 ring buffer」策略位。

---

## 2. 分层边界：这套日志「为谁服务」？

依赖方向（自上而下）：`apps → modules → components → core → platform`（见分层文档）。

### 2.1 原则

1. **业务语义（模块名、级别、字段）** 应对 **全栈统一**：应用、Module、Component 都使用同一套宏/API。
2. **Platform（OSAL/HAL）** 处于最底层，**不得依赖** `components` 里的 spdlog/zlog；否则产生反向依赖。
3. **可行做法**：在 **core** 或 **platform** 定义 **极薄的日志门面 + 可注入 sink**（C 兼容），由 **应用或 `components/Log`** 在启动时 **注册实际后端**（绑定 spdlog/zlog）。

简记：

| 层级 | 能否直接打业务日志 | 说明 |
|------|-------------------|------|
| **apps / modules / components** | ✅ 主要使用者 | 调用统一 `LOG_*` / `oe_log_*` |
| **core** | ✅ 若存在公共代码 | 同上 |
| **platform / OSAL / HAL** | ⚠️ **仅通过薄封装** | 调用 `oe_log` C API 或 **可选** `OE_PLATFORM_LOG` 宏；**不** include spdlog |
| **第三方库** | 由适配层承接 | 不强制改第三方源码 |

**边界结论**：**「统一日志」服务的是整个 OpenEmber 软件栈的观测性**；**底层库只使用与实现无关的门面**，具体写文件/控制台由 **进程初始化阶段** 注入。

---

## 3. 对外使用形态：`LOG_I` / `LOG_E` 等

### 3.1 推荐形态

- **C++**：`LOG_I("...")`、`LOG_E("...")` 或带模块：`LOG_I_MOD("perception", "...")`。
- **C**：`OE_LOG_INFO(OE_LOG_TAG_DEFAULT, "fmt", ...)` 或每个 `.c` 定义 `static const char *OE_LOG_TAG = "hal.uart";`。

### 3.2 建议携带的上下文（可配置）

- **级别**、**时间**、**线程 id**（可选）、**模块/标签**、**文件:行**（Debug 构建）。
- **进程名 / PID**（多进程排障时由后端或 sink 统一加）。

---

## 4. 模块名 / TAG：要不要像 `MODULE_NAME` 一样每个文件写？

### 4.1 实践

- **编译期标签**（与当前 `LOG_TAG` 类似）：每个编译单元或子目录定义 `OE_LOG_TAG` / `LOG_TAG`，**零运行时注册成本**，适合嵌入式。
- **运行时 logger 名（spdlog）**：`spdlog::get("module_name")` 或 `default_logger` + pattern 中带 `%n`；适合需要 **动态创建模块** 的场景。
- **zlog**：`zlog_get_category("name")` 与配置文件中的 category 对应。

### 4.2 OpenEmber 建议

1. **默认**：**静态 TAG 字符串**（宏展开），与仓库目录或模块名一致，例如 `openember.hal.uart`。
2. **可选**：**模块注册表**（启动时注册 `module_id → display_name`），用于 **Web/日志查询** 展示，**不替代** 底层 TAG。
3. **多进程**：TAG **相同**时靠 **PID + 进程可执行名** 区分；或由部署约定 **每进程不同日志文件**。

**不需要** 每次写日志前都「注册一遍」；**初始化**在进程级完成一次即可（见 §6）。

---

## 5. 初始化：谁调用、调用几次？

### 5.1 进程模型

- **每个进程**：**至多一次**「正式」初始化（配置级别、输出路径、格式、异步队列等）。
- **线程**：**不要**每线程 init；logger 一般为 **进程全局**，spdlog/zlog 的线程安全由库保证（按文档使用）。

### 5.2 典型启动顺序

1. `main` 最早阶段：解析配置 / 环境变量 / Kconfig 生成的默认值。
2. 调用 `oe_log_init(...)` 或 `openember::log::Init(...)`，注册 sink、设置级别。
3. 之后各模块直接使用 `LOG_*`，**无需**再 init。

### 5.2.1 重要注意：未初始化不会落盘

- **必须先初始化**（`oe_log_init(<process_name>)` 或 legacy `log_init(<process_name>)`），spdlog 后端才会创建并注册 sinks（stdout / file / syslog）。
- 若只调用 `LOG_I/LOG_E` 而 **没有**调用初始化：
  - 可能仍能在 stdout 看到输出（取决于默认 logger 是否存在），但 **文件落盘/轮转很可能不会生效**。
  - 因此对所有可执行节点（`apps/**`）建议在 `main()` 的早期统一加一行初始化。

### 5.3 库代码（OSAL/HAL）

- **禁止**在库的任意函数里隐式 `init`（避免重复与顺序问题）。
- 若未 init 时调用：可实现 **no-op** 或 **stderr 简易输出**（仅 Debug），**Release** 可断言或丢弃并计数（策略可配）。

### 5.4 动态加载模块（插件）

- 插件 **同属一个进程**：共享已初始化的全局 logger；插件可使用 **自己的 logger 名**（`spdlog::get`）或统一 TAG 前缀 `plugin.<name>`。

---

## 6. 推荐实现路线图（与 zlog/spdlog 的关系）

### 阶段 A（建议先做）

1. 新增 **`components/Log`（或 `core/log`）**：仅 **头文件宏 + 一个 C++ 实现文件** 对接 **spdlog**。
2. 提供 **`include/openember/log.h`（C++）** 与 **`include/openember/log_c.h`（C ABI）**：C 侧仅声明 `oe_log(int level, const char *tag, const char *fmt, ...)`（或变体）。
3. **Platform** 内如需打印：只 **调用 C ABI**（实现在上层链接单元），或通过 **弱符号/空实现** 在应用链接时替换。

### 阶段 B（可选）

- Kconfig：`OPENEMBER_LOG_BACKEND_SPDLOG` / `ZLOG` / `NONE`。
- zlog 适配器仅实现 **与 spdlog 相同签名的 C ABI**，业务宏不变。

### 阶段 C（运维）

- 文档约定：**systemd journal**、**rsyslog**、**文件路径**、**logrotate**。
- 可选：向 **MQTT/HTTP** 上报关键级别（与 `web_dashboard` 日志页对接）。

### 6.1 spdlog 落盘目录与权限（运维必读）

当启用 spdlog 文件 sink（`OPENEMBER_SPDLOG_ENABLE_FILE=y`）时，日志将写入：

- **目录**：`OPENEMBER_SPDLOG_FILE_DIR`
- **文件名**：`<process_name>.log`（`process_name` 来自 `oe_log_init()`/`log_init()` 参数）
- **轮转**：按大小轮转（`OPENEMBER_SPDLOG_ROTATE_MAX_MB` / `OPENEMBER_SPDLOG_ROTATE_MAX_FILES`）

常见问题与处理建议：

- **目录不存在**：
  - 程序会尝试创建目录；但若父目录不存在或不可创建（例如 `/data` 未挂载/不存在），则无法落盘。
- **没有写权限**：
  - 以普通用户运行时，写 `/var/log/openember` 通常需要 root 权限或预先授权。
  - 你可以临时用 `sudo` 验证，但量产/运维更推荐：
    - 使用 systemd unit 配置 `User=` 并配合 `LogsDirectory=` / `RuntimeDirectory=`（systemd 负责创建并赋权），或
    - 通过 `tmpfiles.d` / 部署脚本提前 `mkdir -p` 并 `chown/chmod` 到运行用户，或
    - 将目录设置到设备可写分区（例如 `/data/log/openember`、`/mnt/data/log/openember`），并确保该分区在服务启动前挂载完成。

---

## 7. 与现有代码的衔接

- 当前工程中已有 `LOG_I` / `LOG_TAG` 用法（如 `web_dashboard/main.cpp`），迁移时：
  - **统一宏名与头文件**；
  - **MODULE_NAME** 可保留为 **默认 TAG**，或改为更长的 **分层名**（`apps.services.web_dashboard`）。

---

## 8. 小结表

| 问题 | 建议答案 |
|------|----------|
| zlog + spdlog 同时兼容？ | **门面统一、后端优选其一**；另一者作可选适配，避免业务双 API。 |
| 库还是服务？ | **库**；合并多进程日志靠系统或后续守护进程，非首版必选。 |
| OSAL/HAL 能用吗？ | **能**，通过 **C ABI / 门面**，**不**直接依赖 `components` 里的具体后端。 |
| 模块名怎么写？ | **静态 TAG 为主**；可选运行时注册表用于展示。 |
| 初始化几次？ | **每进程一次**；库内不隐式 init。 |

---

*本文档随实现迭代更新；具体 API 命名与目录以代码与 Kconfig 为准。*
