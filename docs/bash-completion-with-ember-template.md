# Bash 命令补全教程（以 ember 极简模板为例）

本文用一个最小可用的 `ember` 脚本，讲清楚三件事：

- Bash 补全是怎么工作的
- `source <(./scripts/ember completion bash)` 到底做了什么
- 脚本里“按需提权”（`sudo`）应该怎么写才安全、好用

> 目标：只保留 **两个子命令**，方便你在任何项目快速复用。

---

## 1. 补全的原理（先有函数，再绑定命令）

Bash 补全本质上是两步：

1. 定义一个补全函数（例如 `_ember_complete`）
2. 用 `complete -F _ember_complete ember` 把函数绑定到命令

当你输入 `ember <TAB>` 时，Bash 会调用 `_ember_complete`，并把当前输入上下文放进这些变量：

- `COMP_WORDS`：当前命令行被拆分后的数组
- `COMP_CWORD`：光标所在单词索引
- `COMPREPLY`：函数需要返回的候选项数组

---

## 2. 最小模板：只有两个子命令

下面给出一个极简模板，只有 `build` 和 `clean` 两个子命令。

### 2.1 `scripts/ember`（极简 CLI）

```bash
#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<EOF
Usage:
  ember build
  ember clean
  ember completion bash
EOF
}

cmd="${1:-}"
case "${cmd}" in
  build)
    echo "[ember] building..."
    ;;
  clean)
    echo "[ember] cleaning..."
    ;;
  completion)
    shell_name="${2:-bash}"
    if [[ "${shell_name}" == "bash" ]]; then
      cat "$(dirname "$0")/completions/ember.bash"
    else
      echo "unsupported shell: ${shell_name}" >&2
      exit 1
    fi
    ;;
  *)
    usage
    exit 1
    ;;
esac
```

### 2.2 `scripts/completions/ember.bash`（极简补全）

```bash
#!/usr/bin/env bash

_ember_complete() {
  local cur
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"

  if [[ "${COMP_CWORD}" -eq 1 ]]; then
    COMPREPLY=( $(compgen -W "build clean completion" -- "${cur}") )
    return 0
  fi

  if [[ "${COMP_WORDS[1]}" == "completion" && "${COMP_CWORD}" -eq 2 ]]; then
    COMPREPLY=( $(compgen -W "bash" -- "${cur}") )
    return 0
  fi
}

complete -F _ember_complete ember
```

---

## 3. `source <(./scripts/ember completion bash)` 的前因后果

这条命令可以拆成两部分理解：

1. `./scripts/ember completion bash`  
   让你的 CLI 输出一段“补全脚本文本”（就是上面的 `ember.bash` 内容）

2. `source <(...)`  
   Bash 的“进程替换”（process substitution）语法。`<(...)` 会把命令输出变成一个临时可读文件路径，`source` 再把这段脚本加载到**当前 shell**。

所以它的效果是：**当前终端立即拥有 `ember` 的补全能力**，无需重启终端。

为什么一定要 `source`？  
因为补全函数和 `complete -F` 必须注册到“当前 shell 会话”里，子进程里注册不会影响你的终端。

---

## 4. 临时生效 vs 永久生效

临时生效（当前 shell）：

```bash
source <(./scripts/ember completion bash)
```

永久生效（新终端自动生效）：把下面这段写进 `~/.bashrc`：

```bash
if command -v ember >/dev/null 2>&1; then
  source <(ember completion bash)
fi
```

---

## 5. 脚本内提权：推荐“按需提权”，不推荐整条 sudo

很多人会写：

```bash
sudo ./scripts/ember install ...
```

这会导致脚本在 root 上下文里运行，常见副作用是把补全配置写到 `/root/.bashrc`，普通用户无感知。

更好的方式是：**整条脚本按普通用户运行，只在需要写受限目录时调用 sudo**。

示例：

```bash
target="/usr/local/bin/ember"
src="/path/to/scripts/ember"

if ln -sfn "${src}" "${target}" 2>/dev/null; then
  echo "installed without sudo"
else
  sudo ln -sfn "${src}" "${target}"
  echo "installed with sudo"
fi
```

这样能保证：

- 用户配置（例如 `~/.bashrc`）仍写回当前用户
- 只有“文件落盘到系统目录”这一步提权
- 行为更可控，排障更直观

---

## 6. 一套推荐实践（可直接抄）

- CLI 提供 `completion bash` 子命令，输出补全脚本文本
- 补全脚本只做一件事：根据 `COMP_WORDS` 填 `COMPREPLY`
- 安装逻辑默认走用户目录（如 `~/.local/bin`）
- 支持 `--prefix`，但仅在写受限目录时局部 `sudo`
- 永远避免 `sudo ./your-cli ...` 这种整条提权方式

---

## 7. 快速自测清单

- 输入 `ember <TAB>` 是否出现 `build clean completion`
- 执行 `source <(./scripts/ember completion bash)` 后是否立即生效
- 新开终端是否自动生效（若写入了 `~/.bashrc`）
- 安装到 `/usr/local/bin` 时，是否仅链接步骤提权
- `~/.bashrc` 是否写在当前用户而不是 root

---

如果你后续想扩展到 `zsh`，整体思路不变：CLI 继续提供 `completion <shell>`，只是输出的补全脚本语法改成 zsh 对应格式即可。
