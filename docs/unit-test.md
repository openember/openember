# 单元测试

OpenEmber 的单元测试使用 CTest + [MinUnit](https://github.com/siu/minunit) 宏搭配使用。MinUnit 是一个由纯 C 语言实现的简而精的单元测试套件，可以很好地和 CMake 搭配使用。

下面是一个简单的单元测试示例：

```c
#include "minunit.h"

MU_TEST(test_check) {
    mu_check(5 == 7);
}
MU_TEST_SUITE(test_suite) {
    MU_RUN_TEST(test_check);
}

int main(int argc, char *argv[]) {
    MU_RUN_SUITE(test_suite);
    MU_REPORT();
    return MU_EXIT_CODE;
}
```

单元测试由 Kconfig `test/Kconfig` 中的 `CONFIG_OPENEMBER_ENABLE_TESTS`（映射为 CMake `TESTS_ENABLED`）或直接使用 `-DTESTS_ENABLED=ON` 控制，**默认关闭**（正式产物通常不需要编测试）。开启后执行 `cmake --build …` 会编译测试程序，`ctest` 或 `make test` 会运行 `add_test` 注册用例。

## 宿主机回归基线

对应 [优化计划阶段 1](architecture/openember-optimization-plan.md)。使用 Linux 原生构建和独立的 `build-regression/`，不修改 `.config` 或产品 `build/`。CMake 初始缓存只提供默认值，应使用新的构建目录，不能把已有产品缓存视为固定回归配置。

需要 C/C++ 工具链、CMake（支持 FindPython3）、Python 3、Protobuf 编译器 / C++ runtime，以及构建 Zenoh 所需的 Rust / Cargo。Python 测试仅使用标准库。第三方依赖继续使用现有下载和缓存机制。

```bash
cmake -S . -B build-regression -C cmake/RegressionDefaults.cmake \
  -DOPENEMBER_MSGS_SOURCE=LOCAL \
  -DOPENEMBER_MSGS_LOCAL_SOURCE="$(realpath ../openember-msgs)"
CARGO_BUILD_JOBS=2 cmake --build build-regression --parallel 2
ctest --test-dir build-regression -L regression --output-on-failure
ctest --test-dir build-regression -L integration --repeat until-fail:3 --output-on-failure
```

示例明确使用本地 openember-msgs。实际路径可替换；也可显式使用 FETCH，但 latest 会漂移。复现报告应记录 OpenEmber 和 openember-msgs 的 commit、本地差异、工具链版本及 CMakeCache，而不是仅记录“latest”。这不是锁定全部第三方依赖的构建清单。

`CARGO_BUILD_JOBS` 单独约束 Rust 编译，避免只限制 CMake 并发而仍耗尽内存。这里固定使用 2，不修改 `ember build` 的全局策略。

| 测试 | 覆盖 |
| --- | --- |
| `minunit_example`、`test_algorithm`、`test_log`、`test_osal`、`test_actuator` | 现有单元基线，含 Mock 命令失联安全输出 |
| `regression_hardware` | 非法配置拒绝；三类传感器消息；关节命令改变状态；停止发送后安全输出；硬件服务停止及再次启动 |
| `regression_supervision` | 失败重启及次数上限；正常退出不重启；启动心跳超时；运行中进程被终止后重启；监督器退出清理子进程 |

单元测试具有 `unit;regression` 标签；进程测试具有 `integration;regression` 标签。普通构建按已启用的目标和 Endpoint 条件注册测试；回归配置额外检查关键目标和 Endpoint，避免缺失测试却误认为完整基线通过。交叉编译不注册宿主机进程测试。

### 隔离与失败定位

- 硬件示例当前固定使用 `127.0.0.1:7447`。运行前必须空闲；端口占用会失败，测试不会终止占用端口的其他进程。CTest resource lock 仅协调同一次 CTest 中使用相同锁的测试，不提供系统级端口预留，勿同时运行多个回归实例。
- 监督测试使用临时端口、临时 launch 配置和 PID 文件，启动的只是测试工作进程，不加载产品 launch 配置。
- 进程通过私有进程组管理；正常结束和测试异常都会清理自有进程。测试不访问实际执行器，不要求 root。
- 测试等待有期限，失败时输出子进程日志；CTest 将输出保存在 `build-regression/Testing/Temporary/LastTest.log`。临时配置与原始日志随后清理。
- 这组测试未验证 systemd / OS 重启、router 故障恢复、真机 watchdog、真实断电或硬实时行为；这些属于后续阶段。

### 验证记录

2026-09-16：使用上述独立构建命令，全量构建通过；CTest 7/7 通过；两个 integration 测试各连续运行三次通过。硬件测试覆盖 SIGTERM 和 SIGINT 正常退出。

环境：Linux x86_64、GCC 13.3、CMake 3.28.3、Python 3.12.3、Protobuf 3.21.12、Rust 1.93.0。OpenEmber 基于 `4225624` 加本轮修改；openember-msgs 基于 `db6bdf6` 加本地未提交的 sensor / actuator / power 协议及构建修改，**仅检出该 commit 不足以复现此基线**。本轮未修改 openember-msgs。
