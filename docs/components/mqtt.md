# MQTT 组件（`openember::mqtt::Client`）

实现位于 `components/mqtt/`，基于 **Eclipse Paho MQTT C** 同步客户端（`MQTTClient`）。与 `msg_bus_*` 消息总线 API 无关，仅提供独立的 MQTT 发布/订阅封装。

## 依赖与构建

- **Paho MQTT C**：由 CMake `FetchContent`/VENDOR 拉取并**静态链接**进 `libopenember_mqtt.a` 及最终可执行文件；`ldd` **不会**出现 `libpaho-*.so`，属正常现象。若出现 `libssl.so` / `libcrypto.so`，说明 TLS 已链到运行时 OpenSSL。
- **TLS（`ssl://`）与 OpenSSL**：Paho 的 TLS 由 **OpenSSL** 实现；启用 TLS 时静态库会链接 **`ssl`/`crypto`**（运行时仍依赖系统 `libssl.so` / `libcrypto.so`，与是否静态编 Paho 无关）。
  - **构建**需要 **OpenSSL 开发包**（如 `libssl-dev` / `openssl-devel`），以便 CMake 找到头文件与导入库。
  - **Kconfig** `OPENEMBER_MQTT_PAHO_TLS`（默认 **y**）为 **y** 时：CMake 对 OpenSSL 使用 **`find_package(OpenSSL REQUIRED)`**，未安装开发包会直接配置失败，避免误生成无 TLS 的 Paho。
  - 为 **n** 时：强制 `PAHO_WITH_SSL=OFF`，仅适合只连明文 `tcp://` 的 Broker。
  - **未通过 Kconfig 配置**（仅命令行 CMake）时：仍按「尽量自动检测」逻辑，找不到 OpenSSL 时降级为无 TLS 并给出警告。

安装 OpenSSL 后若仍无法用 TLS，请 **删除构建目录**中与 Paho 相关的缓存后重新配置（否则可能仍使用旧的无 SSL 产物），例如：

源码与构建目录形如 `build/_deps/openember_paho_mqtt_c_v1_3_16-src`（版本号与 `cmake/Dependencies.cmake` 中 `OPENEMBER_PAHO_MQTT_C_VERSION` 一致）。升级版本或清理缓存时可删除：

```bash
rm -rf build/_deps/openember_paho_mqtt_c_v*-build build/_deps/openember_paho_mqtt_c_v*-src build/_deps/openember_paho_mqtt_c_v*-subbuild
cmake -S . -B build
cmake --build build
```

## Broker URI 约定

| 场景 | URI 示例 | 说明 |
|------|----------|------|
| 明文 MQTT | `tcp://host:1883` | 默认端口 1883，无 TLS |
| TLS MQTT | `ssl://host:8883` | 常见 EMQX Cloud 等 **8883** 为 **MQTT over TLS**，必须用 **`ssl://`**，**不能用** `tcp://host:8883`（后者是明文 TCP，与 TLS 端口不兼容） |

Paho 支持的 scheme 由编译时是否定义 **`OPENSSL`** 决定；无 OpenSSL 时仅接受 `tcp`/`mqtt`/`ws` 等，**`ssl://` 会在 `MQTTClient_create` 直接失败**。

## `ClientConfig` 要点

- `server_uri`：见上表。
- `client_id`、`username`、`password`：与 Broker 侧配置一致；EMQX 的「客户端认证」用户名/密码填在 `username`/`password`。
- `ssl_trust_store`：服务端 CA 的 **PEM 文件路径**（EMQX 控制台下载的 CA）。为空时：若 `ssl_verify_peer == true`，则依赖 Paho/OpenSSL 默认信任链；若仍校验失败，可显式指定 CA 文件。
- `ssl_verify_peer`：是否校验服务端证书。生产环境建议保持开启；仅本机调试可临时关闭（不推荐上线）。

## Kconfig（menuconfig）

**`Component` → `MQTT (Eclipse Paho C)`**（仅与 **编译/链接 Paho** 相关）：

| 符号 | 含义 |
|------|------|
| `OPENEMBER_COMPONENT_MQTT` | 是否拉取并编译 Paho、编译 `openember_mqtt`。 |
| `OPENEMBER_MQTT_PAHO_TLS` | 是否启用 OpenSSL 构建 Paho（`ssl://`）；为 **y** 时 CMake 对 OpenSSL 使用 **REQUIRED**（需 `libssl-dev`）。 |
| `OPENEMBER_MQTT_PAHO_ASYNC` | 是否在链接时包含 **MQTTAsync**（`paho-mqtt3a(s)`）。`openember::mqtt::Client` 只用同步 **MQTTClient**，默认 **n** 可少链一个库、略减体积；若工程内使用异步 API 再开。 |

**`Application` → `examples`**：在 **`OPENEMBER_EXAMPLE_MQTT_EMQX`** 下配置 **主机名**、**传输方式**（MQTT over TLS `ssl://` 与 **WebSocket over TLS** `wss://`）、**端口**（EMQX Cloud 常见 **8883 / 8084**）、可选 **WSS 路径**（默认 `/mqtt`）、以及客户端 ID、用户名/密码、主题、CA、是否校验证书。若填写 **「Optional: full broker URI」** 非空，则优先使用该完整 URI，忽略主机/端口/传输组合。

`.crt` 与 `.pem` 多为同一种 PEM 文本；CA 路径必须对**运行进程**可读（仅 `root` 可读会导致非 root 运行失败）。

示例依赖 **`OPENEMBER_COMPONENT_MQTT=y`**。运行 `scripts/kconfig/genconfig.sh` 后，`build/config.cmake` 写入上述开关；`genconfig` 对示例字符串优先读 `CONFIG_OPENEMBER_MQTT_EMQX_*`，若无则回退旧版 `CONFIG_OPENEMBER_MQTT_*`（不含 `EMQX` 前缀）以便迁移。

## 常见错误码（`Client::connect()` 返回值）

`connect()` 在失败时返回 **负数 errno** 或对 Paho 返回码取负再取反后的 **正数**（便于与历史行为一致）。示例程序会调用 `openember::mqtt::connect_return_hint(rc)` 给出中文说明。

| 日志中的 `rc` | 含义（简要） |
|----------------|--------------|
| **14** | **`MQTTCLIENT_BAD_PROTOCOL`**：在 **`MQTTClient_create`** 阶段 URI 被拒绝。常见原因：**使用了 `ssl://`，但当前 Paho 未启用 OpenSSL**（CMake 曾未找到 OpenSSL）。与 CA 路径、证书内容无关，需先按上文 **安装 OpenSSL 并清理 Paho 构建缓存后重编**。 |
| **10** | **`MQTTCLIENT_SSL_NOT_SUPPORTED`**：库未带 SSL，同上。 |
| **1** | **`MQTTCLIENT_FAILURE`**：通用失败，例如网络不可达、TLS 握手失败、认证失败等。 |

若 `.config` 中已是 `ssl://` 且 CA 路径正确，但仍出现 **rc=14**，优先检查 **二进制是否由带 OpenSSL 的 Paho 链接**（`cmake` 配置日志中应有 OpenSSL，且 `build/_deps/.../paho-mqtt3cs` 相关目标存在）。

## 参考

- EMQX Cloud：连接地址与端口（8883 / 8084 等）以控制台为准。
- Paho C SDK 文档：<https://www.eclipse.org/paho/clients/c/>
