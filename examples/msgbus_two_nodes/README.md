# msgbus_two_nodes

双进程演示 **`openember::msgbus::MsgBusNode`**（C++ 门面，底层为当前选定的 msgbus：LCM / NNG 等）。

先启动订阅端，再启动发布端：

```bash
./msgbus_demo_subscriber
./msgbus_demo_publisher
```

可选传入与当前 msgbus 后端一致的地址（LCM 一般为 `udpm://239.255.76.67:7667?ttl=1`）；未传参时使用 CMake 注入的默认地址。
