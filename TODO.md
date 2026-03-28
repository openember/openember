# To Do List

- [x] 搭建 CMake 工程
- [x] 支持消息队列中间件（Paho-MQTT、Mosquitto、ZeroMQ）
- [x] pub/sub 通信骨架（ember_pubsub：ZMQ 或内置 UDP + 双节点示例 `pubsub_publisher` / `pubsub_subscriber`）
- [ ] 支持数据库存储（SQLite、MySQL）
- [x] 支持 HTTP 服务器（Mongoose）
- [x] 提供前端网页模板（Bootstrap、Vue）
- [x] 支持状态机（FSM）
- [x] 增加子模块管理器（SMM）
- [ ] 支持子模块预注册机制（不走消息通道）
- [ ] 支持 OTA 升级
- [ ] 支持 HAL 接口层
- [ ] 支持数据采集协议
- [ ] 支持数据上报
- [ ] 支持树莓派的交叉编译
- [ ] 支持 GDB + GDB Server 远程调试
- [ ] 添加内部库（MD5、time、random、ringbuffer 等）
- [x] 增加线程池管理
- [ ] 增加内存管理
- [x] 增加日志记录（zlog）
- [x] 日志库切换为 zlog（已删除 EasyLogger）
- [ ] 增加一个 alogd 日志守护进程
- [x] 支持单元测试（CTest，未支持 GoogleTest）
- [ ] 增加基础库功能（文件读写、时间获取、随机数生成、MD5 校验、CRC 校验）
- [ ] 增加算法库（滤波...）
- [x] 增加 CSV 库
- [ ] 完美兼容 C/C++ 混合编程
- [ ] 支持调用 Lua 代码
- [ ] 支持调用 Python 代码
- [ ] OpenEmber 作为 ROS 的下层（ROS 通过 OpenEmber 的通信模块拿数据、控制硬件）


## Platform

### OSAL（Linux 初版迭代路线图）

目标：提供 **稳定 C ABI** 的最小 OS 抽象原语（跨平台可移植、少依赖），供 HAL/core/modules 使用；避免替代 Boost/Qt 等高层框架。

实现顺序（每完成一个模块做一次 commit，并补/跑对应单元测试）：

- [x] **P0**：`cond` / `event`（等待/唤醒 + 超时语义 + `*_query_caps()`）
- [x] **P1**：`semaphore`（进程内为主；后续视需要扩展命名信号量）
- [x] **P1**：`shm`（共享内存 create/open/map/unmap/close/unlink + `*_query_caps()`）
- [x] **P1**：`socket`（至少 Unix domain socket；可选 TCP/UDP；支持超时与 poll/wait）
- [x] **P1**：`pipe` / `fifo`（匿名管道 + 有名管道，适合控制通道/日志/小数据流）
- [x] **P2（谨慎）**：`signals`（薄封装：屏蔽/解除/同步等待；不做复杂分发）
- [x] **P2（可选）**：POSIX `mq_*`（评估必要性；优先使用现有 msgbus/pubsub 体系）

C++ wrapper（保持轻量、RAII，位于 `platform/osal/include/openember/osal/*.hpp`）：

- [x] 为 `cond/event/sem/shm/socket/pipe` 补齐对应的 C++ RAII 封装（不引入新链接依赖）

## Logging 服务第一版（文件采集 + Web 展示）

- [x] 形成方案：阶段 1 采用“文件采集 + Web 流（由 web_dashboard 对外）”
- [x] 新增 `apps/services/logger` 服务（采集 `*.log`，提供本地 `/api/logs`）
- [x] 在 `apps/Kconfig` 增加 logger 构建开关与参数（端口、日志目录）
- [x] 更新 `scripts/kconfig/genconfig.sh` 映射 logger 相关配置到 CMake
- [x] 在 `apps/CMakeLists.txt` 按开关编译 logger 服务
- [x] `web_dashboard` 增加 `/api/logs` 代理到 logger（本地回环）
- [x] `web_root` logs 页面接入 `/api/logs` 并显示最近日志
- [x] 编译验证（至少 logger + web_dashboard + 现有 apps）

## Logging 阶段 1（主线）：spdlog topic sink + web_dashboard 订阅

- [ ] 在 `components/Kconfig` 增加 spdlog topic sink 配置项（enable/topic/level/rate limit）
- [ ] 在 `scripts/kconfig/genconfig.sh` 映射 topic sink 配置到 CMake cache
- [ ] 在 `components/Log/log_spdlog.cpp` 实现 topic sink（基于现有 pub/sub 骨架发布到 `/openember/log`）
- [ ] 在 `web_dashboard` 增加“订阅日志 topic 并缓存”的后台逻辑（内存 ring）
- [ ] 前端 logs 页面接入（优先走 `/api/logs` 读取 ring；后续再做 SSE）
- [ ] 编译运行验证：至少 `web_dashboard` 能实时看到 `INFO+` 日志

## Msgbus 插件化重构（进行中）

- [x] 修复 `LCM` 默认 topic URL（避免 `LCM provider "tcp" not found`）
- [x] spdlog topic 默认 URL 与 `OPENEMBER_PUBSUB_BACKEND`（ember_pubsub 实际传输）对齐，避免 ZMQ pubsub + LCM msgbus 混用时误用 `udpm://`
- [x] 在 `components/msgbus` 引入 C++ 抽象层骨架（接口 + 工厂 + legacy 适配）
- [ ] 将 `components/msgbus` 旧 C wrapper 逐步迁移到 backend 类（LCM/NNG/ZMQ）
- [ ] 将 `components/msgbus` 的 `mqtt_*` wrapper 迁出到 `components/mqtt`
- [ ] 在 `components/mqtt` 提供独立 C++ 封装（面向 IoT 云连接）
- [ ] 增加 backend 插件注册机制（按 Kconfig/CMake 组合装配）
- [ ] 补齐回归测试（logger/web_dashboard/topic/msgbus 示例）