# third_party/

仓库**不**提交上游源码树；本目录用于**离线归档缓存**（与 `cmake/Dependencies.cmake` 中的版本钉死配合）。

## 命名

将上游发布的压缩包放在此目录，文件名与解压后的顶层目录一致：

- `paho.mqtt.c-1.3.16.tar.gz` 或 `paho.mqtt.c-1.3.16.zip` → 解压后为 `paho.mqtt.c-1.3.16/`

## 行为（以已实现 Paho 为先例）

- **FETCH**：若此处已有对应版本归档，则**不再联网**，直接解压到 `build/_deps/<name>-<version>/` 并构建；缺失时下载到本目录再解压。
- **VENDOR**：请将所需版本的 `.tar.gz` / `.zip` 放入本目录（无需事先解压）；构建时解压到 `build/_deps/`。亦可设置 CMake 缓存 `PAHO_MQTT_C_LOCAL_SOURCE` 指向已解压目录（覆盖归档逻辑）。

其他依赖仍可能使用 `download/_extracted/` 或 FetchContent 旧路径，迁移计划见 `TODO.md` 与 `docs/third-party-management.md`。

## Git

默认可将 `*.tar.gz` / `*.zip` 加入 `.gitignore`（体积大）；若团队希望提交固定归档，可去掉对应忽略规则。
