# 从 AimRT 学习第三方库管理模式（FetchContent）

## 1. AimRT 的组织方式（核心模式）

AimRT 在 `cmake/` 目录下为每个第三方依赖都放置一个独立脚本，例如：

- `GetPahoMqttC.cmake`
- `GetIceoryx.cmake`
- `GetZenoh.cmake`
- `GetOpenTelemetryCpp.cmake`

每个脚本都实现同样的“工程化流程”，让上层 CMake 只需要 `include(GetXxx.cmake)` 或调用对应函数，就能获得可 link 的 CMake target。

## 2. AimRT 的实现细节（版本、构建、引用）

### 2.1 固定版本（可追溯）

脚本里通常会以 `*_DOWNLOAD_URL` 的形式固定版本，例如使用：

- GitHub release tag 的 `archive/<tag>.tar.gz`
- 或更复杂的 tag 变量拼接（如 `v${VERSION}`）

关键点：版本信息在一个位置集中维护，利于升级和审计。

### 2.2 离线/本地源码覆盖

几乎所有 `GetXxx.cmake` 都支持一个可选变量：

- `<dep>_LOCAL_SOURCE`

当设置了本地路径时，会用 `FetchContent_Declare(SOURCE_DIR <path> OVERRIDE_FIND_PACKAGE)` 走本地源码；否则使用 `URL ...` 下载归档。

这让“企业离线场景”不需要改逻辑，只需改变量值。

### 2.3 构建流程可重复且避免重复执行

脚本普遍会使用：

- `FetchContent_GetProperties(<dep>)`
- `if(NOT <dep>_POPULATED) ... FetchContent_MakeAvailable(<dep>) ... endif()`

并且经常把逻辑包在 `function(get_xxx)` 里，以限制变量污染、提升可读性与可维护性。

### 2.4 依赖的开关/选项在 MakeAvailable 前完成

AimRT 会在 `FetchContent_MakeAvailable` 前通过 `set(... CACHE BOOL ...)` 指定上游的构建选项，例如：

- 关闭测试/示例/bench
- 选择 shared/static
- 打开需要的特性（如 opentelemetry 的 http client 等）

这样可以减少构建时间并降低引入额外子模块的风险。

### 2.5 上层引用方式：统一别名 target（可预测）

MakeAvailable 后，脚本会检查上游实际产出的 target，并提供“稳定、统一命名”的 alias 目标。

示例风格包括（以 Paho 为代表）：

- `add_library(paho_mqtt_c::paho-mqtt3c ALIAS paho-mqtt3c)`

OpenTelemetry CMake 中会创建大量 `opentelemetry-cpp::...` alias，便于上层按语义 link。

好处：

- OpenEmber 不需要关心上游 target 命名差异
- 依赖切换或升级后，OpenEmber 的 link 行为尽量保持不变

## 3. 我们在 OpenEmber 可借鉴什么（建议迁移路线）

当前 OpenEmber 已有一个 `cmake/Dependencies.cmake`，但仍偏“集中式变量注入”（传 `*_INCLUDE_DIRS` / `*_LIBRARIES`）。

建议逐步向 AimRT 的“脚本化封装 + alias target”靠拢：

1. 为关键依赖拆出独立脚本：`cmake/GetZlog.cmake`、`cmake/GetCjson.cmake`、后续再加 `GetPahoMqttC.cmake`
2. 每个脚本提供 `<dep>_LOCAL_SOURCE`，让离线场景只改变量
3. 每个脚本在 `MakeAvailable` 后创建 alias target，例如：
   - `ZLOG::ZLOG`
   - `cJSON::cJSON`
4. 顶层 `Dependencies.cmake` 只做“模式选择”（FETCH/VENDOR/SYSTEM）与 include 调度
5. 核心库逐步改为 `target_link_libraries(... PRIVATE ZLOG::ZLOG)` 这种方式，减少 include/lib 变量传递

## 4. 下一步建议（你确认后我可直接开始改代码）

优先迁移范围（小步快跑）：

- 把 `zlog`、`cJSON` 从当前 `Dependencies.cmake` 的实现迁到 `GetZlog.cmake` / `GetCjson.cmake`
- 保持对现有代码的兼容：先仍输出 `ZLOG_INCLUDE_DIRS` / `ZLOG_LIBRARIES`，同时新增 alias target
- 然后再逐步把核心库链接方式切到 target alias

