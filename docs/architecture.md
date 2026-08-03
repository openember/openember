# Architecture

## 架构设计文档

- [Application Layer 设计指导手册](architecture/application-layer-design-guide.md)
- [Runtime Supervision 与 Product App 设计方案](architecture/runtime-supervision-and-product-app-design.md)
- [框架分层设计文档](architecture/layered-architecture-design-guide.md)


## 分层结构

```bash
OpenEmber (规范层)
├── spec/             # 语言无关规范（最核心资产）
|   ├── model.md
|   ├── qos.md
|   └── transport-abstraction.md
|
├── modules/
|   ├── core-c-adapter/       # C ABI 适配层（用于纯 C 接入模板）
|   └── core-cpp/             # C++ 主线实现
├── tools/            # 脚手架 & 工具（可选的 IDL / 代码生成工具）
└── examples/
```



## 关键设计：构建隔离

统一放在一个仓库管理，不靠语言区分项目，而是靠 **构建配置区分**。

核心思想：一个仓库，多种 build profile

### 示例：CMake 顶层设计

```cmake
option(OPENEMBER_ENABLE_CPP        "Enable C++ runtime" ON)
option(OPENEMBER_ENABLE_C_ADAPTER "Enable C ABI adapter" OFF)

option(OPENEMBER_MISRA_MODE "Enable MISRA-C constraints" OFF)
```

用户选择：

```bash
# C 接入模板 + MISRA
cmake -DOPENEMBER_ENABLE_CPP=OFF \
      -DOPENEMBER_ENABLE_C_ADAPTER=ON \
      -DOPENEMBER_MISRA_MODE=ON ..

# 现代 C++
cmake -DOPENEMBER_ENABLE_CPP=ON \
      -DOPENEMBER_ENABLE_C_ADAPTER=OFF ..
```
