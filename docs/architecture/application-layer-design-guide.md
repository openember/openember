# OpenEmber Application Layer 设计指导手册

本文档定义 OpenEmber 框架中 **Applications 层（应用层）** 的职责范围、目录组织方式、推荐节点分类结构，以及框架自带节点与用户节点之间的边界原则。

目标：

> 建立一个长期可扩展、跨行业通用、支持机器人/无人系统/工业控制/智能终端/AI Agent 场景的应用层设计规范。



## 一、Applications 层的定位

Applications 层是 OpenEmber 分层架构的最顶层。

其职责不是提供基础能力，而是：

```
组合组件能力
组织运行节点
实现系统行为
完成具体业务逻辑
```

换句话说：

```
components = 能力
modules = 功能
applications = 行为
```

Applications 层描述的是：

> 系统最终“做什么”

而不是：

> 系统“有什么能力”



## 二、Applications 层设计原则

必须遵守以下原则：

### 原则 1：Applications 只做编排，不做能力实现

例如：

正确：

```
启动 perception_node
调用 navigation_node
连接 planner_node
```

错误：

```
在 app 中实现导航算法
在 app 中实现视觉算法
```

算法应属于：

```
modules
components
```

### 原则 2：Applications 只依赖 modules/components/core

依赖关系应始终保持：

```
applications
    ↓
modules
    ↓
components
    ↓
core
```

禁止反向依赖。

### 原则 3：Applications 不绑定具体行业

错误结构：

```
apps/
   robot/
   drone/
   agv/
   security/
```

正确结构应基于功能角色划分。



## 三、Applications 推荐目录结构

推荐采用如下结构：

```bash
apps/

├── system/        # 系统基础节点
├── references/    # 参考实现节点
├── tools/         # 开发辅助工具节点
├── services/      # 系统服务节点
└── examples/      # 最小示例节点
```

说明如下。



### apps/system（系统基础节点）

system 目录用于存放框架运行所必需的基础服务节点。

典型包括：

```
launch_manager
log_service
config_service
health_monitor
node_registry
lifecycle_manager
time_service
device_manager
```

这些节点属于：

```
框架基础运行设施
```

类似：

```
systemd
roscore
supervisor
```



### apps/services（系统服务节点）

services 提供运行时功能支持，但不是框架必须组件。

典型包括：

```
web_dashboard
rest_api_server
rpc_gateway
ota_update_service
metrics_exporter
telemetry_service
```

这些节点通常用于：

```
远程管理
监控
调试
升级
集成平台
```



### apps/tools（开发辅助工具节点）

tools 目录用于开发调试辅助工具。

典型包括：

```
topic_echo
topic_record
node_inspector
system_profiler
bandwidth_monitor
latency_checker
```

类似 ROS2 的：

```
ros2 topic echo
ros2 bag
```

这些节点主要用于：

```
开发阶段
调试阶段
测试阶段
```

为什么必须区分 services 和 tools？

因为它们生命周期不同：

| 类型     | 生命周期         |
| -------- | ---------------- |
| services | runtime长期运行  |
| tools    | 调试阶段临时运行 |

例如：

```bash
web dashboard → services
topic echo → tools
```



### apps/references（参考实现节点）

references 目录用于提供官方推荐的示例节点组合方式。

例如：

```
apps/references/

├── perception/
├── control/
├── navigation/
├── communication/
├── ai/
└── agent/
```

这些节点用于展示：

```
如何使用框架能力
如何组合模块
如何构建系统流程
```

未来用户会直接复制 `apps/references/*` 来构建产品系统，这就是 framework adoption accelerator 🚀



### apps/examples（最小示例节点）

examples 用于教学和快速验证。

典型包括：

```
hello_node
ping_node
camera_demo
ipc_demo
```

目标：

```
降低学习成本
验证运行环境
演示接口使用方式
```

examples 和 references 的区别：

| 类型       | 面向对象 |
| ---------- | -------- |
| examples   | 初学者   |
| references | 工程师   |



## 四、为什么不建议创建 apps/robot、apps/drone、apps/agv

原因如下：

行业目录不可扩展：

```
robot
agv
drone
vehicle
security
factory
medical
home
```

数量无限增长。

正确方式是：

按能力角色划分：

```
perception
planning
control
communication
agent
```

行业差异应由用户项目承担。



## 五、用户自定义节点应该放在哪里？

原则：

```
用户节点不进入 OpenEmber 主仓库
```

推荐结构：

```
workspace/

├── openember/
├── user_apps/
└── user_modules/
```

例如：

```
my_robot_project/

├── openember/
├── apps/
├── modules/
└── components/
```

这种方式类似：

```
ROS2 workspace
```



## 六、Applications 层推荐节点类型分类

推荐按照运行角色划分：

```
infra nodes
service nodes
tool nodes
reference nodes
example nodes
```

说明如下。



## 七、Infra Nodes（基础设施节点）

例如：

```
log_collector
config_server
health_monitor
lifecycle_manager
```

作用：

```
支撑系统运行稳定性
```

属于：

```
apps/system
```



## 八、Service Nodes（服务节点）

例如：

```
web_ui
rest_api
ota_updater
metrics_exporter
```

作用：

```
提供系统外部访问能力
```

属于：

```
apps/services
```



## 九、Tool Nodes（调试工具节点）

例如：

```
topic_echo
node_monitor
perf_tracker
```

作用：

```
提高开发效率
```

属于：

```
apps/tools
```



## 十、Reference Nodes（参考实现节点）

例如：

```
vision_pipeline_node
navigation_pipeline_node
agent_demo_node
```

作用：

```
展示框架能力组合方式
```

属于：

```
apps/reference
```



## 十一、Example Nodes（最小示例节点）

例如：

```
hello_world_node
ipc_demo_node
camera_demo_node
```

作用：

```
帮助用户快速入门
```

属于：

```
apps/examples
```



## 十二、Applications 层与 Agent Runtime 的关系

Applications 层未来可以承载：

```
agent entry node
workflow entry node
launch entry node
```

例如：

```
assistant_agent_node
robot_agent_node
monitor_agent_node
```

它们属于：

```
apps/reference/agent
```

而不是：

```
components/agent
```

因为它们是行为入口。



## 十三、Applications 层设计总结

Applications 层核心职责：

```
组织系统行为
组合模块能力
提供运行入口
支持开发调试
展示最佳实践
```

推荐目录结构最终版：

```
apps/

├── system/
├── services/
├── tools/
├── reference/
└── examples/
```

该结构具备以下优势：

```
跨行业通用
可长期扩展
支持 Agent Runtime
支持机器人系统
支持工业控制
支持无人系统
支持智能终端
支持教学示例
支持用户二次开发
```

适合作为 OpenEmber 官方 Applications 层长期标准结构。

