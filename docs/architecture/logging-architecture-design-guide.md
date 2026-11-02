# OpenEmber Runtime 日志系统设计指导文档（Logging Architecture Design Guide）

OpenEmber Logging System is designed as a distributed, topic-based runtime observability pipeline with optional centralized log control service.

文档目标是回答三件事：

1. 为什么这样设计？
2. 系统整体结构是什么？
3. 每一层分别负责什么？

并确保未来扩展到 **机器人产品级 runtime / fleet 管理系统** 时不需要推翻架构。



## 一、设计目标（Design Goals）

OpenEmber 日志系统不是简单的 `printf()` 封装，而是 runtime 基础设施的一部分，其设计目标包括：

核心目标：

- 统一日志接口
- 支持多进程节点架构
- 支持实时 dashboard 可视化
- 支持模块级过滤
- 支持远程调试能力
- 支持未来 fleet logging 扩展

工程目标：

- 低耦合
- 高性能
- 可扩展
- 跨平台
- 嵌入式友好

运行时目标：

- 实时日志流
- 历史日志缓存能力
- 动态日志等级控制
- 日志订阅能力

一句话总结：OpenEmber Logging 是 Runtime Observability Infrastructure，而不是简单日志库 📡



## 二、设计原则（Design Principles）

OpenEmber Logging 遵循以下原则：

1️⃣ Logging API 统一入口原则

所有模块统一使用：

openember_log.h

例如：

OE_LOG_INFO("init success");

而不是：

printf
std::cout
syslog
spdlog directly

优势：

可替换 backend
统一格式
统一等级控制
统一输出策略
2️⃣ Topic-first Logging 架构原则

日志默认进入 topic：

/openember/log

原因：

Topic 是 runtime 系统最自然的数据总线结构：

实时
低延迟
可订阅
可过滤
可扩展

参考：

ROS 2 的 /rosout

3️⃣ Logger Service 可选增强原则

logger service：

不是必须组件

而是：

optional observability enhancement daemon

启动：

增强日志能力

未启动：

系统仍正常运行

类似：

journald
rosout daemon
4️⃣ Dashboard 直接订阅日志原则

默认路径：

node
 ↓
topic
 ↓
dashboard

而不是：

node
 ↓
logger
 ↓
dashboard

优势：

降低延迟
减少复杂度
减少组件依赖



## 三、整体架构（Overall Architecture）

OpenEmber Logging Pipeline：

Application
Runtime
Component
HAL
OSAL
   ↓
OpenEmber Log API
   ↓
spdlog backend
   ↓
topic sink
   ↓
/openember/log
   ↓
├── web_dashboard
└── logger_service (optional)

其中：

topic sink = primary transport layer
logger service = control plane



## 四、日志系统分层设计（Layered Logging Architecture）

OpenEmber Logging 结构分为四层：

Logging API Layer
Logging Backend Layer
Logging Transport Layer
Logging Control Layer
1️⃣ Logging API Layer

位置：

components/log/

接口：

OE_LOG_TRACE
OE_LOG_DEBUG
OE_LOG_INFO
OE_LOG_WARN
OE_LOG_ERROR
OE_LOG_FATAL

示例：

OE_LOG_INFO("UART initialized");

职责：

统一日志接口
模块标识管理
日志等级控制入口
2️⃣ Logging Backend Layer

默认 backend：

spdlog

原因：

header-only
高性能
支持多 sink
支持嵌入式
支持 syslog
支持 rotation

输出结构：

console sink
file sink
syslog sink
topic sink

示例：

spdlog
 ├── stdout sink
 ├── file sink
 ├── syslog sink
 └── topic sink
3️⃣ Logging Transport Layer（核心设计）

新增：

topic sink

输出：

/openember/log

日志格式建议：

{
  "ts": 1712233445,
  "level": "INFO",
  "module": "hal.uart",
  "thread": 12,
  "msg": "init success"
}

优势：

实时
可订阅
可过滤
可远程桥接
4️⃣ Logging Control Layer（logger service）

组件：

openember_loggerd

职责：

不是日志转发器，而是：

log controller

功能：

日志缓存
日志过滤
日志等级控制
日志导出
日志历史回放
日志远程同步

结构：

topic listener
syslog listener
log cache
websocket server



## 五、Dashboard 日志链路设计（Dashboard Log Pipeline）

推荐结构：

node
 ↓
topic sink
 ↓
/openember/log
 ↓
web_dashboard subscribe

Dashboard 直接订阅 topic：

优势：

实时
低延迟
无中间层依赖

logger service 仅增强能力：

history replay
filter
search
remote debug



## 六、为什么不用文件轮询日志？

不推荐：

node → log file → logger

原因：

IO效率低
rotation复杂
延迟高
同步困难
不可扩展

工业 runtime 很少使用该方案。



## 七、为什么不用共享内存作为日志主链路？

共享内存适合：

进程间通信

不适合：

dashboard 日志流

原因：

浏览器无法访问
同步复杂
调试困难
不可远程访问

推荐：

topic
socket
websocket



## 八、为什么 logger service 不是必须组件？

因为：

topic sink 已经提供：

实时日志流
dashboard订阅能力
跨节点日志统一入口

logger service 负责：

控制能力增强

而不是：

基础日志能力

设计原则：

topic = data plane
loggerd = control plane

这是现代 runtime 标准架构。



## 九、Web Dashboard 通信设计

Dashboard 使用：

Mongoose

支持：

HTTP
WebSocket
MQTT
TCP

推荐路径：

loggerd
 ↓
WebSocket
 ↓
dashboard frontend

示例：

ws://localhost:19100/log

前端：

ws.onmessage = render_log

无需额外后端转发层。



## 十、Syslog 在 OpenEmber 中的角色

推荐启用：

syslog sink

用于：

系统日志统一接入
远程日志支持
Linux工具兼容

可选增强：

syslog-ng

作用：

日志收集
日志转发
日志远程同步

关系：

spdlog
 ↓
syslog
 ↓
syslog-ng

不是 logger service 替代品。



## 十一、推荐默认日志端口规划

建议：

19100 logger websocket
19101 metrics
19102 dashboard api

避免冲突：

8000
8080
3000
5000



## 十二、最终日志系统架构总结（Final Architecture）

OpenEmber Logging Runtime：

OpenEmber Log API
        ↓
      spdlog
        ↓
   topic sink
        ↓
  /openember/log
        ↓
 ├── web_dashboard
 └── logger_service(optional)
        ↓
 websocket(optional)
        ↓
 dashboard frontend

扩展路径：

syslog sink
remote logging
fleet logging
cloud observability



---

一句话总结：

OpenEmber Logging is designed as a topic-centric runtime log streaming architecture with an optional centralized log control service for advanced observability and remote debugging. 🚀