# 双节点 pub/sub 示例

基于 `libs/pubsub` 的 **ember_pubsub** API，演示两个进程之间的发布与订阅。

## 传输层

- **UDP（内置）**：默认 `udp://127.0.0.1:7560`，无额外依赖。
- **ZeroMQ（libzmq）**：启用 `-DBUILD_PUBSUB_ZMQ=ON`，默认 `tcp://*:5560` / `tcp://127.0.0.1:5560`。
- **NNG（nanomsg-next-gen）**：启用 `-DBUILD_PUBSUB_NNG=ON`，默认 `tcp://127.0.0.1:5560` / `tcp://127.0.0.1:5560`。
- **LCM**：启用 `-DBUILD_PUBSUB_LCM=ON`，默认 provider `udpm://239.255.76.67:7667`（内部固定 channel `openember/pubsub`）。

构建时 CMake 会打印当前选用的传输方式。

## 构建

在工程 `build` 目录中：

```bash
cmake .. -DBUILD_PUBSUB_ZMQ=ON
cmake --build . --target pubsub_publisher pubsub_subscriber
```

可执行文件通常在 `build/bin/`。

## 运行

先启动订阅端，再启动发布端（UDP 下订阅端需先绑定端口；ZMQ 下发布端内置短延迟以缓解慢加入者问题）。

```bash
# 终端 1
./pubsub_subscriber

# 终端 2
./pubsub_publisher
```

可选：自定义 URL（需与 CMake 选用的传输一致）
- UDP/NNG：均为 `udp://...` 或 `tcp://...`
- ZMQ：均为 `tcp://...`
- LCM：provider 字符串（例如 `udpm://...`）

```bash
./pubsub_subscriber udp://127.0.0.1:9000
./pubsub_publisher udp://127.0.0.1:9000
```

## 关闭

`pubsub_subscriber` 为长期运行示例，用 `Ctrl+C` 结束。
