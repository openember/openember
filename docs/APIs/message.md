# 消息中间件

消息中间件的接口位于 libs/MessageAPI 目录，使用相关接口只需要在你的代码中引入头文件，如下所示。

```c
#include "msg_wrapper.h"
```



## 注册和注销

一个节点（无论是线程还是进程）想要通过消息中间件来传递消息，首先需要调用 `msg_bus_init()` 接口进行注册，此时会完成一系列的初始化操作，并连接到消息总线上。

```c
int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb);
```

调用该函数时，需要传入一个句柄指针，后续对消息总线的操作都需要用到，完整的参数和返回值说明见下表。

| **参数**  | **描述**                                         |
| --------- | ------------------------------------------------ |
| handle    | 消息节点的句柄                                   |
| name      | 消息节点的名称                                   |
| address   | 消息总线的地址，传入 `NULL` 表示使用默认消息总线 |
| cb        | 消息处理回调函数，当接收到消息时会自动调用该函数 |
| **返回**  | ——                                               |
| AG_EOK    | 初始化成功                                       |
| -AG_ERROR | 初始化失败                                       |

当不再需要操作消息总线传递消息时，可以调用 `msg_bus_deinit()` 注销并释放消息节点的所有资源。

```c
int msg_bus_deinit(msg_node_t handle);
```

注销接口的参数和返回值见下表。

| **参数**  | **描述**             |
| --------- | -------------------- |
| handle    | 要注销的消息节点句柄 |
| **返回**  | ——                   |
| AG_EOK    | 注销成功             |
| -AG_ERROR | 注销失败             |



## 订阅消息

订阅消息之前需要先设置消息处理回调函数，回调函数的类型定义如下。

```c
typedef void msg_arrived_cb_t(char *topic, void *payload, size_t payloadlen);
```

你可以在调用 `msg_bus_init()` 接口时传入回调函数的入口（函数名称），可以通过 `msg_bus_set_callback()` 接口设置回调函数，函数原型如下。

```c
int msg_bus_set_callback(msg_node_t handle, msg_arrived_cb_t *cb);
```

具体参数和返回值说明见下表。

| **参数**  | **描述**         |
| --------- | ---------------- |
| handle    | 消息节点句柄     |
| cb        | 消息处理回调函数 |
| **返回**  | ——               |
| AG_EOK    | 回调函数绑定成功 |
| -AG_ERROR | 回调函数绑定失败 |

设置好消息处理回调函数以后，就可以订阅消息了，订阅消息的接口如下。

```c
int msg_bus_subscribe(msg_node_t handle, const char *topic);
```

消息通过主题（Topic）来区别不同的消息，因此订阅时需要传入想要订阅的消息主题，完整参数和返回值说明见下表。

| **参数**  | **描述**                                  |
| --------- | ----------------------------------------- |
| handle    | 消息节点句柄                              |
| topic     | 消息主题，字符串类型，例如 `/test/topic1` |
| **返回**  | ——                                        |
| AG_EOK    | 订阅成功                                  |
| -AG_ERROR | 订阅失败                                  |



## 发布消息

当消息节点初始化完成后，我们就可以通过该节点向消息总线发布消息，发布消息的接口函数原型如下。

```c
int msg_bus_publish(msg_node_t handle, const char *topic, const char *payload);
```

调用该函数时，需要传入一个句柄指针，一个消息主题，以及消息的主题，完整的参数和返回值说明见下表。

| **参数**  | **描述**                   |
| --------- | -------------------------- |
| handle    | 消息节点的句柄             |
| topic     | 要发布的消息主题           |
| payload   | 要发布的消息内容（字符串） |
| **返回**  | ——                         |
| AG_EOK    | 发布成功                   |
| -AG_ERROR | 发布失败                   |

`msg_bus_publish()` 接口默认采用字符串（如 JSON 格式）作为 Payload 的格式，如果需要使用透传方式（如结构体）则可以使用 `msg_bus_publish_raw()` 接口，函数原型如下。

```c
int msg_bus_publish_raw(msg_node_t handle, const char *topic, const void *payload, const int payloadlen);
```

完整参数和返回值说明见下表。

| **参数**   | **描述**                       |
| ---------- | ------------------------------ |
| handle     | 消息节点的句柄                 |
| topic      | 要发布的消息主题               |
| payload    | 要发布的消息内容（支持结构体） |
| payloadlen | 要发布消息的长度               |
| **返回**   | ——                             |
| AG_EOK     | 发布成功                       |
| -AG_ERROR  | 发布失败                       |



## 接收消息

和订阅消息绑定回调函数不同，接收消息的接口 `msg_bus_recv` 提供的是同步消息处理机制，也就是说，在 `timeout` 时间内会一直等待，直到有消息返回。

```c
int msg_bus_recv(msg_node_t handle, char** topic, void** payload, int* payloadlen, time_t timeout);
```

完整参数和返回值说明见下表。

| **参数**     | **描述**                                           |
| ------------ | -------------------------------------------------- |
| handle       | 消息节点的句柄                                     |
| topic        | 接收消息主题的变量地址，由调用者传入               |
| payload      | 接收消息内容的变量地址，由调用者传入（支持结构体） |
| payloadlen   | 接收消息长度的变量地址，由调用者传入               |
| timeout      | 超时时间，单位毫秒                                 |
| **返回**     | ——                                                 |
| AG_EOK       | 接收成功                                           |
| -AG_ERROR    | 接收失败（例如设置了消息订阅回调函数）             |
| -AG_ETIMEOUT | 超时返回                                           |



## 检查连接状态

如有需要，可以调用下面接口函数获取某个消息节点与消息总线的连接状态。

```c
int msg_bus_is_connected(msg_node_t handle);
```

参数和返回值说明见下表。

| **参数** | **描述**           |
| -------- | ------------------ |
| handle   | 消息节点句柄       |
| **返回** | ——                 |
| AG_TRUE  | 已连接             |
| AG_FALSE | 未连接（断开连接） |

