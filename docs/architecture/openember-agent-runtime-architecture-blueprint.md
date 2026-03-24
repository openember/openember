# OpenEmber Agent Runtime 架构蓝图（Embodied Agent Runtime 设计文档）

本文档用于定义 OpenEmber 在未来演进为 **Embodied Agent Runtime（具身智能运行时平台）** 的整体设计方向，是框架长期能力路线的重要基础。



## 一、什么是 Runtime？

在软件工程中，Runtime（运行时）指的是：

> 程序运行期间负责调度、管理资源、执行任务、组织模块协作的一整套基础系统能力集合。

它通常负责：

- 进程/线程调度
- 模块生命周期管理
- 插件加载
- 配置系统
- 通信机制
- 内存管理
- 日志系统
- 任务执行流程

例如：

| Runtime         | 作用             |
| --------------- | ---------------- |
| Linux kernel    | 操作系统运行时   |
| JVM             | Java 运行时      |
| ROS2 runtime    | 机器人节点运行时 |
| Node.js runtime | JS 运行时        |

而我们希望构建的是：Embodied Agent Runtime（即：具身智能系统运行时）

**表：runtime vs framework vs middleware 的区别**

| 类型       | 作用             |
| ---------- | ---------------- |
| framework  | 提供开发结构     |
| middleware | 提供通信能力     |
| runtime    | 负责执行系统行为 |



## 二、什么是 Embodied Agent Runtime？

传统 Agent：

```
输入 → LLM → 输出
```

Embodied Agent：

```
传感器 → 感知 → 记忆 → 推理 → 规划 → 执行 → 控制执行器
```

它是一个闭环系统：

```
Perception
Memory
Reasoning
Planning
Execution
Action
Feedback
```

这就是机器人级 Agent Runtime。



## 三、OpenEmber Agent Runtime 的目标定位

目标不是：

```
聊天机器人框架
```

目标是：

```
边缘智能设备运行时平台
```

支持：

- 机器人
- 无人机
- 无人船
- 自动驾驶子系统
- 工业控制系统
- 安防设备
- 智能终端

统一运行架构。

### 为什么 OpenEmber 天然适合成为 Agent Runtime？

因为我们已经设计了：

```bash
transport middleware
HAL abstraction
module system
plugin architecture
launch system
Kconfig configuration
FetchContent dependency manager
```

这些组合起来就是：

```bash
runtime skeleton
```

只需要再补：

```bash
planner
memory
workflow
tool registry
execution graph
```

就完成 Agent Runtime 体系。



## 四、Agent Runtime 总体架构

逻辑结构如下：

```
Applications
    ↑
Agent Graph Runtime
    ↑
Agent Capability Layer
    ↑
OpenEmber Components
    ↑
Transport / HAL / OS
```

说明：

| 层级                   | 作用                     |
| ---------------------- | ------------------------ |
| Applications           | 用户应用节点             |
| Agent Graph Runtime    | Agent 执行流程调度       |
| Agent Capability Layer | 感知/记忆/规划能力       |
| Components             | 通信/日志/配置等基础能力 |
| Transport/HAL          | 硬件接口层               |



## 五、Agent Runtime 目录结构设计（推荐方案）

建议新增 `components/agent/`

结构如下：

```
components/agent/

├── runtime/
├── planner/
├── executor/
├── memory/
├── context/
├── tool_registry/
├── graph_engine/
└── workflow/
```

说明如下。



### Agent Runtime 子模块设计

核心职责：负责 Agent 生命周期管理

包括：

```
Agent 启动
Agent 停止
节点注册
模块加载
事件循环
```

示例接口：

```c++
AgentRuntime
AgentSession
AgentLifecycleManager
```



### planner（规划器）

职责：负责任务拆解

例如：

```
用户指令：拿起杯子
```

规划结果：

```
识别杯子
定位杯子
规划路径
执行抓取
```

模块示例：

```c++
TaskPlanner
BehaviorPlanner
LLMPlanner
```



### executor（执行器）

职责：执行 Planner 生成的任务步骤

例如：

```c++
navigation.goto()
arm.grasp()
```

模块示例：

```c++
ToolExecutor
ActionExecutor
CommandDispatcher
```



### memory（记忆系统）

Agent 的核心能力之一

支持：

```
短期记忆
长期记忆
语义记忆
任务记忆
设备状态记忆
```

示例接口：

```c++
AgentMemory
VectorMemory
StateMemory
TaskMemory
```

可接入：

```c++
sqlite
faiss
milvus
```



### context（上下文管理）

负责：

```
上下文窗口管理
Prompt构建
多轮对话状态
环境状态同步
```

示例接口：

```c++
ContextManager
PromptBuilder
SessionContext
```



### tool_registry（工具注册中心）

Agent 的核心机制之一：**Tool Calling**

例如：

```
CAN控制
导航接口
HTTP接口
数据库接口
视觉接口
```

注册方式：

```c++
ToolRegistry
registerTool()
callTool()
```



### graph_engine（执行图引擎）

负责执行：

```
Agent执行流程图
```

例如：

```
Camera → Detection → LLM → Navigation
```

示例接口：

```c++
ExecutionGraph
GraphScheduler
NodeExecutor
```

类似：

```c++
ROS2 executor
```



### workflow（工作流系统）

用于支持：

```
多步骤任务执行
任务恢复
任务暂停
任务回滚
```

示例接口：

```c++
WorkflowEngine
WorkflowInstance
WorkflowScheduler
```



## 六、modules 层扩展设计

建议新增：

```
modules/agent/
```

结构如下：

```
modules/agent/

llm/
planner/
executor/
tools/
perception_bridge/
```

示例：

```
modules/agent/llm/openai
modules/agent/llm/qwen
modules/agent/llm/ollama
```



## 七、components/ai 与 components/agent 的关系

关系如下：

```
ai = inference capability
agent = decision capability
```

结构关系：

```
components/

ai/
agent/
```

职责区别：

| 模块  | 职责     |
| ----- | -------- |
| AI    | 推理能力 |
| Agent | 决策能力 |



## 八、Agent Graph Runtime（核心执行机制）

Agent 本质运行模型：

```
Graph Execution
```

例如：

```
CameraNode
 → DetectionNode
 → PlannerNode
 → ExecutorNode
```

OpenEmber Launch 系统未来可以扩展为：

```
Agent Graph Loader
```

支持：

```
.launch.yaml
.agent.yaml
.graph.yaml
```



## 九、Agent Runtime 与 Launch 系统关系

关系如下：

```
Launch = 进程级调度
AgentRuntime = 任务级调度
```

示例：

```
Launch启动节点
AgentRuntime调度行为
```



## 十、Agent Runtime 与 Kconfig 集成方案

建议新增：

```
menuconfig AGENT_RUNTIME
```

结构建议：

```
Agent Runtime

[ ] Agent Graph Engine
[ ] Planner Engine
[ ] Memory Engine
[ ] Workflow Engine
[ ] Tool Calling Support
```



## 十一、推荐未来版本路线图

建议三阶段演进：

Phase 1：

```
components/ai
```

Phase 2：

```
modules/agent
```

Phase 3：

```
components/agent/runtime
```

最终目标：

```
OpenEmber = Embodied Agent Runtime Platform
```



## 十二、最终架构定位总结

OpenEmber 未来目标定位：

```
Edge Intelligent Middleware Platform
```

能力包括：

```
Hardware abstraction
Transport middleware
Module runtime
AI inference
Agent reasoning
Workflow scheduling
Tool execution
Application orchestration
```

这意味着 OpenEmber 将成为：

```
机器人 + 自动驾驶 + 工业智能 + 安防 + IoT
统一智能运行时平台
```



如果继续沿这条路线演进：

OpenEmber 可以成为：

```
Edge Intelligent Agent Runtime Platform
```

类似：

```
ROS2 + LangChain + Triton + systemd
```

的统一版本。

但：

```
更轻量
更模块化
更嵌入式友好
更可裁剪
```

这在当前生态里几乎没有成熟对手。