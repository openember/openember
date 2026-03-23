# third_party/

OpenEmber 已不再在仓库中落库 `cJSON` / `zlog` / `paho.mqtt.c` / `sqlite` 等上游源码。

第三方依赖统一由：

- `cmake/Dependencies.cmake`（版本钉死、FETCH / VENDOR / SYSTEM）
- `cmake/Get*.cmake`（FetchContent 拉取与构建）

如需离线构建，请将对应版本解压到 `download/_extracted/`（路径与 `Dependencies.cmake` 中 VENDOR 默认一致），并将 `OPENEMBER_THIRD_PARTY_MODE` 设为 `VENDOR`。
