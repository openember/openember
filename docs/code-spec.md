# 编码规范

## 注释

### 行内注释

注释就是程序的解释性语句，注释是写给人看的，用以提高源代码的可读性，编译器在预编译阶段会忽略所有注释的内容。

无论是单行注释还是多行注释，都需要使用 `/* ... */` 形式，建议使用英文注释，例如：

```c
int a = 0; /* This is a comment line */

/*
 * There are some comment line
 */

```

### 文件头注释

```c
/*
 * Copyright (c) 2017-2022, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-01-01     luhuadong    the first version
 */
```

### 函数注释

```c
/**
 * This function will do something ...
 *
 * @param arg1 the first parameter
 * @param arg2 the second parameter
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int foo(int arg1, int arg2)
{
    /* do something */
}
```

## 标识符命名

标识符（Identifiers 或 symbols）是程序中的变量、类型、函数和标签提供的名称，

## 空白符

### 缩进

统一使用 4 空格缩进。


### 头文件引用

规则：

1. `#include` 和头文件之间有且只有一个空格
2. 系统提供的头文件用尖括号 `<>` 包含，表示外部来源
3. 工程内部的头文件用双引号 `""` 包含，表示内部来源

以下写法符合要求：

```c
#include <stdio.h>
#include <iostream>
#include "myself.h"
```

以下写法不符合要求：

```c
#include<stdio.h>
#include  <iostream>
#include "stdio.h"
#include <myself.h>
```

### 宏定义


```c
#define ON 1
```