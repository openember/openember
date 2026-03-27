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

### 6.0 syslog / journald / syslog-ng：`OPENEMBER_SPDLOG_ENABLE_SYSLOG` 的作用

当 `OPENEMBER_SPDLOG_ENABLE_SYSLOG=y` 时，spdlog 会启用 **syslog sink**，把日志写入系统的 syslog 设施（facility 默认为 `LOG_USER`）。

- **它本身不是服务**：它只是把日志“交给系统日志通道”。\n
- **是否能对接 syslog？** 可以。syslog sink 输出的内容会被系统的日志栈接收：\n
  - 在不少发行版中，**journald** 会收集 syslog 消息；\n
  - 若系统启用了 **rsyslog** 或 **syslog-ng**，它们可以从 `/dev/log`（Unix socket）接收并按规则落盘、转发、过滤、聚合。\n
- **为什么要这个选项**：在“多进程、多节点”场景下，syslog/journald/syslog-ng 能提供**统一采集、统一落盘、统一轮转、统一转发**，比每个进程各写一套文件更利于运维与审计。

注意：是否真的“进 journald”取决于目标系统的日志栈配置；OpenEmber 只负责把消息写到 syslog 通道。

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

### 6.2 我们是否需要“自己的 logger 服务”？（建议与边界）

你提到的 **syslog-ng** 本质上就是“日志服务/管道”的一种成熟实现：集中收集、多源输入、过滤、落盘、转发与检索对接。\n
因此是否需要 OpenEmber 自己再做一个 `logger` 进程，取决于我们想补齐哪些能力（以及是否愿意重复造轮子）。

#### 6.2.1 不做自研 logger 的推荐路径（更工程化）

- **落盘**：用本项目内置的 **rotating file sink**（每进程一个文件）或直接走 **syslog sink**。\n
- **集中采集/转发**：交给系统侧（journald / rsyslog / syslog-ng / Fluent Bit / Vector）。\n
- **Web 展示**：web_dashboard 不直接去“尾读文件”，而是读取**聚合后的流**（见下一节）。

优点：少维护一个守护进程，可靠性与生态更好；缺点：依赖目标系统具备/配置好日志栈。

#### 6.2.2 什么时候需要自研 logger 服务（可选，后期再做）

当你需要下面这些“设备级”能力，而且目标系统无法方便配置现成日志栈时，可以考虑自研：

- **统一的跨进程 ring buffer**：限制磁盘写放大，支持断网/离线缓存。\n
- **设备侧“日志策略”**：按模块/级别动态下发（运行时变更级别、采样、限速）。\n
- **统一的 Web/SSE/WebSocket 日志流**：把多进程日志汇聚成一个可订阅流。\n
- **签名/审计/防篡改**：运维合规需求（可选）。\n

但它的代价也很真实：需要处理输入协议、背压、丢弃策略、磁盘满、重启恢复、性能与安全等。

#### 6.2.3 “写到一个文件”是不是必须？

不必须。**每进程一个文件**往往更易定位（配合进程名/PID），也更少争用。\n
如果确实要“一个文件”，更推荐用系统日志栈（syslog-ng/rsyslog）做聚合写入，或用一个专门的 logger 服务做“单写者”。

### 6.3 日志发送到 Web：各节点直发 vs 发布订阅聚合

你提到的两种方式都可行，但建议明确边界与扩展方向：

#### 6.3.1 各节点直接发送到 web_dashboard（不推荐作为默认）

- **缺点**：每个进程都要实现网络协议/重试/鉴权；web_dashboard 一挂，全栈都要处理失败；运维策略分散。\n
- **适用**：极少量关键事件上报（例如告警），而不是全量日志流。

#### 6.3.2 发布订阅聚合（推荐作为默认演进方向）

建议引入一个逻辑概念：**log topic**（例如 `openember/log/<level>` 或 `openember/log`），由日志模块提供一个“可选 sink”把日志条目发布到 msgbus/pubsub。\n

- **logger 服务（可选）**：订阅 log topic，负责落盘/转发/过滤。\n
- **web_dashboard（可选）**：也订阅同一个 log topic，通过 SSE/WebSocket 推送到前端 logs 页面。\n

这样做的好处是：
- 生产者（各节点）只需“写日志”，不关心日志去哪。\n
- 消费者（logger/web）可插拔并可扩展多个。\n
- 可在 msgbus 层做背压/限速/采样策略（视具体实现）。\n

**注意**：不要用 msgbus 转发“全量 debug 日志”作为默认，建议只推 `INFO+` 或可配置阈值，避免占用总线带宽。

---

## 6.5 重新评估（推荐主线）：spdlog + topic sink + 可选 loggerd（ROS2/rosout 风格）

结合 OpenEmber 已有的通信骨架（topic/pubsub），相比“logger 读文件再转发”的路径，你提出的 **topic sink** 更符合工程直觉，也更容易形成清晰的分层：

```
spdlog
 ↓ (topic sink)
/openember/log
 ↓
├── web_dashboard（直接订阅）
└── openember_loggerd（可选：日志控制中心/缓存/过滤/导出）
```

### 6.5.1 核心结论（策略建议）

- **topic sink**：作为 OpenEmber logging 的主线能力（类比 ROS2 `rosout`）
  - 建议 **构建层面始终具备**（不要求业务改造）
  - 建议 **默认开启发布**，但默认只发布 `INFO+`（或可配置阈值），避免把 DEBUG 全量打到总线上
- **openember_loggerd**：作为 **optional daemon**
  - **启动**：自动增强日志能力（控制平面 + 数据缓存 + 导出）
  - **不启动**：系统仍可运行（stdout/file/syslog 仍可用，dashboard 也可直接订阅 topic）
- **web_dashboard**：
  - 阶段 1 只做“实时日志展示”，直接订阅 `/openember/log`
  - 回看/导出/检索属于阶段 2+（由 loggerd 提供）

### 6.5.2 三阶段演进（与你的规划对齐）

#### 阶段 1（当前版本，最小可上线：无需 loggerd）

```
spdlog
 ↓ topic sink
/openember/log
 ↓
web_dashboard
```

目标：
- dashboard 可实时看到全系统 `INFO+` 的日志
- 同时保留 stdout/file/syslog 作为运维落地与诊断兜底

#### 阶段 2（增强：引入 loggerd 作为控制平面）

```
spdlog
 ↓ topic sink
/openember/log
 ↓
├── web_dashboard（订阅展示）
└── openember_loggerd（缓存/过滤/导出/动态调级）
```

`openember_loggerd` 价值（不是“转发日志”，而是 **Log Control Plane**）：
- **log controller**：动态调级（按模块/级别/规则），采样、限速、黑白名单
- **log cache**：内存 ring（可选落盘索引）支持回看
- **log filter**：level/tag/module/regex/time range
- **log exporter**：下载导出、远端转发（后续）

#### 阶段 3（产品级：loggerd 统一对外，dashboard 走 WebSocket/SSE）

```
spdlog
 ↓ topic sink
/openember/log
 ↓
openember_loggerd
 ↓ WebSocket/SSE
dashboard
```

并扩展：
- syslog bridge / remote logging / fleet logging

### 6.5.3 topic sink “永远开启”如何做到优雅（可控且不扰民）

推荐采用“编译能力常驻 + 发布策略可控”的模式：

- **编译层面**：topic sink 代码始终存在于 `components/Log`（不要求业务层改动）
- **发布策略**（Kconfig 默认值注入，后续可由 loggerd 下发覆盖）：
  - **阈值**：默认 `INFO+`
  - **限速**：例如每秒 N 条（0 表示不限制）；超限丢弃并计数
  - **采样**：阶段 2 做（对重复日志采样/合并）

这样满足：
- loggerd 未启动：topic 仍可用（dashboard 可直接订阅）
- loggerd 启动：自动增强能力（控制/缓存/导出/对外接口）

### 6.5.4 日志事件数据模型（先简单，后可扩展）

建议首版 topic payload 用 **单行 JSON**（便于跨语言与调试）：

```json
{"ts":"2026-03-27T12:34:56.789Z","lvl":"info","pid":1234,"proc":"web_dashboard","tag":"apps.services.web_dashboard","msg":"Listening on 0.0.0.0:8000"}
```

字段建议：
- `proc`：来自 `oe_log_init(process_name)`
- `tag`：来自 `LOG_TAG`
- `msg`：格式化后的最终字符串（首版就够用）
- `ts/pid`：由 sink 填充（避免业务侧额外成本）

### 6.5.5 Kconfig：stdout/file/syslog/topic 四 sink 的组合建议

当前已有：
- `OPENEMBER_SPDLOG_ENABLE_STDOUT`
- `OPENEMBER_SPDLOG_ENABLE_FILE`
- `OPENEMBER_SPDLOG_ENABLE_SYSLOG`

建议补充（新增）：
- `OPENEMBER_SPDLOG_ENABLE_TOPIC`（bool）
- `OPENEMBER_SPDLOG_TOPIC_NAME`（string，默认 `"/openember/log"`）
- `OPENEMBER_SPDLOG_TOPIC_LEVEL_CHOICE`（choice：debug/info/warn/error，默认 info）
- `OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT`（int，每秒最大条数；0 表示不限制）

默认值建议：
- stdout：y（开发友好）
- file：y（运维落地）
- syslog：n（按系统日志栈决定）
- topic：y（主线能力），但 level 默认 `info`

### 6.5.6 运行注意事项：端口 `bind: 98`（Address already in use）

你遇到的：

```
mg_open_list bind: 98
mg_listen Failed: 127.0.0.1:18081
```

说明 **端口已被占用**（同机已有进程监听 `127.0.0.1:18081`）。  
在我们后续把 loggerd 做成可选 daemon 后，建议：
- 端口做成 **Kconfig 可配**（并支持禁用对外监听，仅走 topic）
- 或在同机多实例时用 `--port`/环境变量覆盖（阶段 2 再实现）

### 6.4 备选方案：logger 不订阅 topic，而是“采集日志文件”再统一对外

你提出的方案是可行的，核心是把 logger 当成 **collector + gateway**：

1. **输入**：logger 监控各进程日志文件（tail/follow）
2. **处理**：解析、打统一字段、过滤、缓存
3. **输出**：统一提供给 web_dashboard（SSE/WebSocket/API），或再发布到 topic

#### 6.4.1 文件采集方案的优缺点

**优点**
- 与业务进程低耦合：业务只负责写日志文件，不感知总线/网络
- 对历史日志友好：logger 可以读取“已有文件”，支持回放
- 与现有 spdlog 文件落盘能力直接兼容

**缺点/复杂点**
- 需要处理轮转（rename/truncate）与多文件跟随（`inotify + offset` 状态机）
- 需要处理解析容错（多行日志、截断行、异常编码）
- 日志实时性受文件 flush 策略影响（不是严格实时）
- 在高吞吐下，文件 I/O + 解析会有额外开销

#### 6.4.2 与“直接订阅 topic”对比结论

- **实时性**：topic 通常更好
- **实现复杂度（首版）**：文件采集更容易先跑通；topic 需要先定义日志事件协议
- **可靠性**：文件采集天然可回放；topic 需要额外持久化策略
- **系统耦合**：文件采集更依赖文件系统与轮转行为；topic 更依赖总线稳定性

#### 6.4.3 推荐的折中架构（建议落地）

建议采用 **“输入走文件，输出走流”** 的两阶段方案：

- **阶段 1（先上线）**  
  - logger 只做文件采集（每进程日志文件）
  - 对外提供 `web_dashboard` 所需实时流（SSE）+ 最近 N 条查询 API
  - 先不引入 topic 入站，降低改造面

- **阶段 2（能力增强）**  
  - 增加可选 topic 入站（让部分关键模块走实时 topic）
  - logger 作为统一出口：仍对 web 提供单一流接口
  - 最终形成“双输入”（file/topic）+“单输出”（web/api/可选再发布）

##### 6.4.3.1 你问的“Web 流”到底是什么意思？

这里的“Web 流”是指 **给前端页面连续推日志** 的机制（通常 SSE 或 WebSocket），并不等价于“必须再开一个对外 HTTP 端口”。

有两种落地方式：

- **方式 A：logger 自己提供 HTTP/SSE 接口**  
  - logger 进程监听本地端口（例如 `127.0.0.1:18080`）或 Unix socket 对外提供 `/api/logs`、`/api/log/stream`  
  - web_dashboard 前端直接请求 logger（同机可走反向代理或同源转发）

- **方式 B（推荐首版）：web_dashboard 继续对外，logger 只做“后端数据源”**  
  - logger 不对外暴露网页端口，只提供本地 IPC 接口（Unix socket）或内部 topic  
  - web_dashboard 的 `main.cpp` 在 `/api/logs` / `/api/log/stream` 里转发/聚合 logger 数据  
  - 好处是外部仍只有一个 Web 入口，部署与鉴权更简单

> 结论：首版建议 **方式 B**。这样不需要再让 logger 成为“第二个 Web 服务器”，对外接口仍由 web_dashboard 统一承载。

##### 6.4.3.2 阶段 1 的最小数据路径（文件采集 + Web）

推荐链路：

`各节点 -> spdlog 文件 -> logger tail/parse -> logger 内存环形缓冲 -> web_dashboard API/SSE -> 浏览器`

其中：
- logger 维护最近 N 条（例如 10k）内存缓冲用于低延迟查询/流推送
- web_dashboard 负责对外 HTTP 与鉴权；logger 只负责日志数据处理
- 历史日志分页可由 logger 从文件 + offset 读取，实时日志从内存缓冲推送

#### 6.4.4 logger 与 web_dashboard 的接口建议

- **推荐**：`web_dashboard` 不直接读文件，不直接订阅各模块
- **统一由 logger 提供**：
  - `GET /api/logs?level=&tag=&since=&limit=`（历史/分页）
  - `GET /api/log/stream`（SSE 实时推送）

这样可以把权限控制、过滤、限速、脱敏都集中在 logger。

##### 6.4.4.1 阶段 2 的“双输入单输出”再解释（你问的重点）

是的，含义就是在阶段 1 基础上增加一个 topic 入站：

- **输入 1（已有）**：文件采集（tail 每进程日志文件）
- **输入 2（新增，可选）**：topic 日志流（各节点发布 `openember/log`）
- **输出（单一）**：仍由 logger 统一提供给 web_dashboard（API/SSE），可选再转发到外部系统

具体方向是：

`节点(可选) --topic--> logger`  
`节点(默认) --file--> logger`  
`logger --统一接口--> web_dashboard`

不是要求“所有节点都必须 topic 化”。建议：
- 默认仍靠文件输入（零侵入）
- 只给少数低时延模块启用 topic sink（可配置）
- logger 做去重/合并策略，避免 file 与 topic 双通路重复显示

##### 6.4.4.2 topic 入站是否会增加节点开销？要不要改 log 组件？

会增加一定开销，但可以控：
- 网络/总线序列化开销
- 消息队列堆积风险

控制方法：
- 默认关闭 topic sink，按模块/级别开启（例如仅 `WARN+`）
- 采样与限速（burst 防护）
- 背压时降级（丢 DEBUG，保 ERROR）

是否要改造日志组件：**需要，但可以很小步**。  
在 `components/Log` 增加一个“可选 sink 插件点”（file/stdout/syslog 之外的 topic sink），先不改所有节点调用方式（仍 `LOG_*`）。

#### 6.4.5 socket / 共享内存作为 logger 输入是否值得？

- **Unix socket**：适合本机实时输入，复杂度中等，后续可作为 file/topic 之外的第三输入
- **共享内存 ring**：性能最好但复杂度最高（同步、丢包、恢复），建议后期再做

结论：如果目标是“尽快可用 + 运维可控”，优先 **文件采集 + SSE/API 输出**；  
若后续追求低延迟与高吞吐，再引入 **topic/socket/shm** 混合输入。

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
