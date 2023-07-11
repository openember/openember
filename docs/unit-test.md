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

单元测试由 CMake 选项开关 `TESTS_ENABLED` 控制，默认开启。执行 `make test` 将会编译并运行 test 目录下的所有测试程序（实际上是使用 `add_test` 函数添加的测试程序）。
