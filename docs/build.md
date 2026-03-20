# OpenEmber 构建方法（CMake）

本文档汇总当前仓库的“推荐构建方式”和常用开关。后续如果你要求我修改/新增构建流程，我会同步更新本文档。

## 1. 前置条件

- Linux 环境
- CMake 版本：项目要求 `cmake_minimum_required(VERSION 3.1.0)`
- C/C++ 编译器：系统默认即可（示例使用 GCC 13）

## 2. 标准构建流程

在工程根目录创建构建目录（示例：`build/`）：

```bash
mkdir -p build
cd build
cmake .. \
  -DEXAMPLES_ENABLED=ON \
  -DTESTS_ENABLED=ON \
  -DOPENEMBER_THIRD_PARTY_MODE=FETCH
cmake --build . -j$(nproc)
```

说明：

- `OPENEMBER_THIRD_PARTY_MODE` 控制第三方库来源：
  - `FETCH`：CMake 运行时下载并构建固定版本
  - `VENDOR`：优先使用仓库/`download/_extracted/` 中已准备好的源码（适合离线/内网）
  - `SYSTEM`：优先使用系统已安装依赖（Yocto/企业镜像场景）

## 3. 选择 pub/sub transport（两节点示例）

当前 `libs/pubsub` 会根据下面三个开关之一启用后端（并构建 `examples/pubsub_two_nodes`）：

- `-DBUILD_PUBSUB_ZMQ=ON`：ZeroMQ（libzmq）
- `-DBUILD_PUBSUB_NNG=ON`：NNG（nanomsg-next-gen）
- `-DBUILD_PUBSUB_LCM=ON`：LCM（lcm）

默认情况下 `BUILD_PUBSUB_ZMQ=ON`，其他两个关闭。

### 3.1 构建 ZMQ 版本示例

```bash
mkdir -p build-zmq && cd build-zmq
cmake .. \
  -DOPENEMBER_THIRD_PARTY_MODE=VENDOR \
  -DBUILD_PUBSUB_ZMQ=ON \
  -DBUILD_PUBSUB_NNG=OFF \
  -DBUILD_PUBSUB_LCM=OFF
cmake --build . --target pubsub_publisher pubsub_subscriber -j$(nproc)
```

### 3.2 构建 NNG 版本示例

```bash
mkdir -p build-nng && cd build-nng
cmake .. \
  -DOPENEMBER_THIRD_PARTY_MODE=VENDOR \
  -DBUILD_PUBSUB_ZMQ=OFF \
  -DBUILD_PUBSUB_NNG=ON \
  -DBUILD_PUBSUB_LCM=OFF
cmake --build . --target pubsub_publisher pubsub_subscriber -j$(nproc)
```

### 3.3 构建 LCM 版本示例

```bash
mkdir -p build-lcm && cd build-lcm
cmake .. \
  -DOPENEMBER_THIRD_PARTY_MODE=VENDOR \
  -DBUILD_PUBSUB_ZMQ=OFF \
  -DBUILD_PUBSUB_NNG=OFF \
  -DBUILD_PUBSUB_LCM=ON
cmake --build . --target pubsub_publisher pubsub_subscriber -j$(nproc)
```

## 4. 运行示例

示例程序通常在 `build/bin/` 下生成，运行时先启动订阅端再启动发布端：

```bash
# 终端 1：订阅端
./pubsub_subscriber

# 终端 2：发布端
./pubsub_publisher
```

如需自定义 URL / provider，请参考：

- `examples/pubsub_two_nodes/README.md`

## 4.1 内部通信骨架（msgbus）= NNG / LCM

`libs/msgbus` 支持在构建期切换内部通信后端（通过 CMake 选项控制）：

- 默认：NNG（`-DOPENEMBER_MSGBUS_USE_NNG=ON`）
  - 工作方式依赖 `msgbus_nng_forwarder` 做跨进程转发
  - forwarder 程序：`build/bin/msgbus_nng_forwarder`
  - 默认端点（IPC）：
    - IN 端点（modules PUB listen -> forwarder SUB dial）：`ipc:///tmp/openember-msgbus-in.ipc`
    - OUT 端点（forwarder PUB listen -> modules SUB dial）：`ipc:///tmp/openember-msgbus-out.ipc`
  - 启动方式（先启动 forwarder，再启动模块）：
    ```bash
    ./msgbus_nng_forwarder
    ./Template
    ```
  - 如需自定义端点，可传参：
    `./msgbus_nng_forwarder <in_url> <out_url>`（即先 IN 再 OUT）

- 可选：LCM（`-DOPENEMBER_MSGBUS_USE_LCM=ON`，并关闭 NNG：`-DOPENEMBER_MSGBUS_USE_NNG=OFF`）
  - LCM 后端不需要额外 forwarder；直接启动模块即可
  - 模块示例：`./Template`

## 5. 交叉编译（可选）

顶层支持：

- `-DCROSSCOMPILE_ENABLED=ON`

该模式会切换编译器/系统根路径到 `aarch64`（当前脚本示例写死了 Petalinux sysroot 路径）。

建议你如果需要正式支持某个目标平台，把 sysroot 路径改成可配置参数，再同步到本文档。

