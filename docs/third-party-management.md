# 第三方库管理（FetchContent / Vendor / System）

## 模式定义

通过 `OPENEMBER_THIRD_PARTY_MODE` 控制第三方库来源：

- `FETCH`：使用 CMake `FetchContent` 下载并构建“固定版本”的依赖
- `VENDOR`：使用仓库内 `third_party/` 的已落库源码
- `SYSTEM`：优先使用系统已安装的依赖（Yocto/企业场景）

## 版本钉死与升级

每个依赖应在 CMake 中用“版本变量 + URL/下载信息”固定到可追溯状态。

升级流程（建议）：

1. 在 `cmake/Dependencies.cmake` 中更新依赖版本变量（以及必要时的校验信息）
2. 重新配置构建：`cmake .. -DOPENEMBER_THIRD_PARTY_MODE=FETCH`
3. 验证目标：编译通过、目标链接正确（include dir / library name）
4. 若升级涉及 ABI 行为变化，更新对应的 OpenEmber 适配层/文档说明

## 当前已接入的依赖（示例范围）

- `zlog`（固定到 `1.2.16`：已支持 Fetch/Vendor/System；若下载的上游 tarball 不包含 CMake 构建文件，会自动回退到仓库内 `third_party/zlog`）
- `cJSON`（固定到 `1.7.15`，已支持 Fetch/Vendor/System）
- `paho.mqtt.c`（固定到 `1.3.13`：已支持 Fetch/Vendor/System）
- （已删除）`easylogger`：当前已不再维护；日志统一走 `zlog`

## 代码接入原则

为避免维护成本膨胀：

- 只为关键能力引入 transport / ABI 适配层所需的最低集合（不强依赖上游测试/示例/语言绑定）
- CMake 层提供统一的 include/lib 变量（例如 `ZLOG_INCLUDE_DIRS`、`ZLOG_LIBRARIES`），让核心代码保持稳定

