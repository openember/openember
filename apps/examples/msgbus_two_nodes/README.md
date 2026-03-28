# msgbus_two_nodes

双进程演示 **msg_bus_***（与 `launch_manager`、`web_dashboard` 等同一套内部总线）。

先启动订阅端，再启动发布端：

```bash
./msgbus_demo_subscriber
./msgbus_demo_publisher
```

可选传入与当前 msgbus 后端一致的地址（例如 LCM 的 `udpm://...`）；默认与 CMake 所选后端匹配。
