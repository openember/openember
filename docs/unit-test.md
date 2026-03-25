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
