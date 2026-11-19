# Transport 接收载荷上限

`transport::TopicOptions` 和 `transport::ServiceOptions` 增加可选 `max_payload_bytes`。零值保留原行为，正值限制收到的单个应用载荷；发送端和上层协议仍需独立校验。

```cpp
openember::transport::TopicOptions topic;
topic.name = "/motion/base/command";
topic.max_payload_bytes = 8192;

openember::transport::ServiceOptions service;
service.name = "/motion/base/request";
service.max_payload_bytes = 8192;
```

Subscriber 在把 Zenoh payload 复制到 C++ ByteBuffer 前检查长度，超限消息不进入应用回调。ServiceServer 在复制请求前检查，超限时返回固定错误文本，不调用应用回调；调用方不得把错误文本当成有效协议回复。ServiceClient 在复制回复前检查，超限返回 kInvalidArgument。相同 ServiceOptions 字段在服务端限制请求、在客户端限制回复，两端应分别配置。

这不是中间件网络缓冲上限，也不提供身份验证、来源授权、消息字段/递归边界、请求队列容量或发送限流。需要这些约束的应用仍应在字节检查之后、Protobuf 解码和派发之前建立相应边界。现有 typed Node 便利接口保持默认行为，要求显式限制的端点可通过 Context.Transport 的原始接口创建。

本次跨进程验证由产品 quadruped_motion_link_boundary 覆盖：超限 RPC 请求和 topic 不进入运动处理，超限 RPC 回复返回错误，小 topic 正常投递，零值回复限制保留旧行为。测试同时覆盖运动协议认证/去重边界；验证入口见 openember-quadruped/docs/motion-link-control.md。
