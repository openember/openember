# Architecture



## 分层结构

```bash
OpenEmber (规范层)
├── spec/             # 语言无关规范（最核心资产）
|   ├── model.md
|   ├── qos.md
|   └── transport-abstraction.md
|
├── modules/
|   ├── core-c/       # C 实现（使用 LCM / NNG 等）
|   └── core-cpp/     # C++ 实现（使用 iceoryx / Zenoh 等）
├── tools/            # 脚手架 & 工具（可选的 IDL / 代码生成工具）
└── examples/
```



## 关键设计：构建隔离

统一放在一个仓库管理，不靠语言区分项目，而是靠 **构建配置区分**。

核心思想：一个仓库，多种 build profile

### 示例：CMake 顶层设计

```cmake
option(EMBER_ENABLE_C   "Enable C runtime" ON)
option(EMBER_ENABLE_CPP "Enable C++ runtime" OFF)

option(EMBER_MISRA_MODE "Enable MISRA-C constraints" OFF)
```

用户选择：

```bash
# 纯 C + MISRA
cmake -DEMBER_ENABLE_C=ON \
      -DEMBER_ENABLE_CPP=OFF \
      -DEMBER_MISRA_MODE=ON ..

# 现代 C++
cmake -DEMBER_ENABLE_CPP=ON ..
```