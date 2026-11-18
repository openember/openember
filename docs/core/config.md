# YAML 运行参数

`components/config` 提供可选目标 `openember::config`，公开头文件为
`<openember/config/yaml.hpp>`。Kconfig / CMake 负责构建模块和依赖，YAML 承担启动时读取的设备参数。
首版没有参数服务器、自动搜索、文件合并、热加载、环境变量替换或 include。

## 构建

启用 `OPENEMBER_USE_YAMLCPP` 与 `OPENEMBER_COMPONENT_CONFIG`，链接 `openember::config`。
组件复用已解析的 yaml-cpp 依赖，公开 API 不暴露该依赖的类型。
CMake 默认随 `OPENEMBER_WITH_YAMLCPP` 启用；显式关闭 config 时不生成目标，
已启用 config 却关闭 YAML 依赖会在配置阶段报错。

```cmake
target_link_libraries(my_node PRIVATE openember::config)
```

## 读取与校验

```cpp
#include <openember/config/yaml.hpp>

struct Settings { double kp, kd; };
Settings ReadSettings(const std::string& path) {
    const auto root = openember::config::LoadYamlFile(path);
    root.CheckKeys({"schema_version", "controller"});
    root.At("schema_version").Require(root.Get<int>("schema_version") == 1,
                                     "unsupported schema version");
    const auto node = root.At("controller");
    node.CheckKeys({"kp", "kd"});
    return {node.At("kp").AsInRange<double>(0, 1000),
            node.At("kd").AsInRange<double>(0, 100)};
}
```

范围与 schema 由消费模块定义。先完成完整参数校验，再启动设备或控制线程。
如需更改运行实例，应用负责原子替换、控制状态检查和失败回退。

| API | 语义 |
| --- | --- |
| `LoadYamlFile(path, options)` | 显式文件路径，读取单个 YAML document |
| `ParseYaml(text, source, options)` | 内存输入，source 用于错误定位 |
| `At(key)` / `At(index)` | 必须存在的字段或序列元素 |
| `Find(key)` | 缺失返回 `std::nullopt`；父节点仍必须是 map |
| `As<T>()` | 标量转换或 `std::vector<T>` 序列转换 |
| `Get<T>(key)` | 必填字段转换 |
| `GetOr<T>(key, fallback)` | 仅在字段缺失时使用 fallback；null/类型错误不会被吞掉 |
| `CheckKeys({...})` | 拒绝未知字段；与 `At` 配合定义 schema |
| `AsInRange<T>(min,max)` / `Require(...)` | 字段范围或模块交叉约束 |
| `Keys()` / `Size()` / `Type()` | 显式遍历结构 |

支持标准整数、bool、string、float/double 及其 vector。整数是十进制，布尔只接受小写
`true` / `false`，浮点值必须有限且在目标类型表示范围内。标量转换按调用者请求的类型进行，
带引号的数字仍能被显式读取为数字。字段路径不做点号分割，`At("a.b")` 访问字面量键 `a.b`。

返回的 Node 使用独立不可变树，子节点可以在根对象释放后继续存活，并允许多线程共享读取。
读取函数不会访问全局参数状态；不同文件之间不隐式继承。

## 错误与文件约定

`ConfigError` 继承 `std::runtime_error`，提供 `Code/Source/Path/Line/Column`，例如：

```text
robot.yaml:4:12 $["controller"]["enabled"]: expected finite number
```

缺失字段使用父节点位置；文件打开失败等无源码位置的错误行列为 0。
错误码区分 I/O、解析、限制、重复键、缺失、类型、非法值及未知字段。

拒绝重复键、复杂 map key、显式 YAML 标签、`<<` 合并键与多个 documents。
普通别名在展开时受资源限制；循环别名最终触发深度限制。
`LoadOptions` 默认最大输入 1 MiB、深度 32（根为第 1 层）、节点数 65536（含根/容器及别名展开后的值，不计 map key）。
所有限制必须为正，深度最高可配置为 128。这些约束不是恶意 YAML 的完整隔离：
底层解析器在生成快照前仍会解析输入。设备配置应来自受信任的配置来源。

不自动解释资源路径；模块要明确路径相对于配置目录、资产根还是工作目录。
已有 `launch_manager` 和 HI hardware YAML 保持各自解析器与 schema，此组件不会自动修改它们。

## 验证

框架 test 构建启用时包含本组件用例，也可以独立构建，无需通信栈或机器人 SDK：

```bash
cmake -S components/config/test -B /tmp/openember-config-tests \
  -DOPENEMBER_YAMLCPP_LOCAL_SOURCE=/absolute/path/to/yaml-cpp-0.8.0
cmake --build /tmp/openember-config-tests -j4
ctest --test-dir /tmp/openember-config-tests --output-on-failure
```

未指定源码路径时使用系统 `find_package(yaml-cpp)`；独立测试不自动下载依赖。
