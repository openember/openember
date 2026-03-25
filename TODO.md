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
- [ ] **P2（可选）**：POSIX `mq_*`（评估必要性；优先使用现有 msgbus/pubsub 体系）

C++ wrapper（保持轻量、RAII，位于 `platform/osal/include/openember/osal/*.hpp`）：

- [ ] 为 `cond/event/sem/shm/socket/pipe` 补齐对应的 C++ RAII 封装（不引入新链接依赖）