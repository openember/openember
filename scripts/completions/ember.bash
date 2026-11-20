#!/usr/bin/env bash
# Bash completion for OpenEmber CLI.

_ember_complete() {
  local cur prev words cword
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  words=("${COMP_WORDS[@]}")
  cword="${COMP_CWORD}"

  local commands="new add del list use current menuconfig genconfig defconfig savedefconfig update configure build all clean run completion install uninstall help -h --help"

  # First positional arg: subcommand.
  if [[ "${cword}" -eq 1 ]]; then
    COMPREPLY=( $(compgen -W "${commands}" -- "${cur}") )
    return 0
  fi

  # new <name> [--path|--model|--openember ...]
  if [[ "${words[1]}" == "new" ]]; then
    if [[ "${prev}" == "--path" || "${prev}" == "--openember" ]]; then
      COMPREPLY=( $(compgen -d -- "${cur}") )
      return 0
    fi
    if [[ "${cur}" == --* ]]; then
      COMPREPLY=( $(compgen -W "--path --model --openember --no-symlink --force --help" -- "${cur}") )
      return 0
    fi
    return 0
  fi

  # add <name> <project_root>
  if [[ "${words[1]}" == "add" ]]; then
    if [[ "${cword}" -eq 2 ]]; then
      # env name (no good generic completion)
      return 0
    fi
    if [[ "${cword}" -eq 3 ]]; then
      COMPREPLY=( $(compgen -d -- "${cur}") )
      return 0
    fi
  fi

  # use <name>
  if [[ "${words[1]}" == "use" && "${cword}" -eq 2 ]]; then
    # best-effort: list names by reading ~/.openember/ember/envs
    local env_dir="${HOME}/.openember/ember/envs"
    if [[ -d "${env_dir}" ]]; then
      COMPREPLY=( $(compgen -W "$(ls -1 "${env_dir}" 2>/dev/null)" -- "${cur}") )
    fi
    return 0
  fi

  # del <name>
  if [[ "${words[1]}" == "del" && "${cword}" -eq 2 ]]; then
    local env_dir="${HOME}/.openember/ember/envs"
    if [[ -d "${env_dir}" ]]; then
      COMPREPLY=( $(compgen -W "$(ls -1 "${env_dir}" 2>/dev/null)" -- "${cur}") )
    fi
    return 0
  fi

  # Second positional arg for completion subcommand: shell name.
  if [[ "${words[1]}" == "completion" && "${cword}" -eq 2 ]]; then
    COMPREPLY=( $(compgen -W "bash" -- "${cur}") )
    return 0
  fi

  # install/uninstall option completion.
  if [[ "${words[1]}" == "install" || "${words[1]}" == "uninstall" ]]; then
    if [[ "${prev}" == "--prefix" ]]; then
      COMPREPLY=( $(compgen -d -- "${cur}") )
      return 0
    fi

    if [[ "${cur}" == --* ]]; then
      COMPREPLY=( $(compgen -W "--prefix" -- "${cur}") )
      return 0
    fi
  fi

  # Product model names belong to the current workspace, not the CLI checkout.
  if [[ "${words[1]}" == build || "${words[1]}" == all ]]; then
    if [[ "${prev}" == -j || "${prev}" == --jobs ]]; then
      COMPREPLY=( $(compgen -W "1 2 4 8 16 32" -- "${cur}") )
      return 0
    fi
  fi
  case "${words[1]}" in
    menuconfig|genconfig|defconfig|savedefconfig|update|configure|build|all|clean|run)
      if [[ "${prev}" == --product ]]; then
        local root="${OPENEMBER_ROOT:-${PWD}}" model models=""
        while [[ "${root}" != / && ! -d "${root}/products" ]]; do
          root="$(dirname "${root}")"
        done
        for model in "${root}"/products/*/product.defconfig; do
          [[ -f "${model}" ]] || continue
          model="${model%/product.defconfig}"
          models+=" ${model##*/}"
        done
        COMPREPLY=( $(compgen -W "${models}" -- "${cur}") )
      elif [[ "${cur}" == -* ]]; then
        local options="--product --build-dir"
        [[ "${words[1]}" != run ]] || options+=" --dry-run"
        if [[ "${words[1]}" == build || "${words[1]}" == all ]]; then
          options+=" -j --jobs"
        fi
        COMPREPLY=( $(compgen -W "${options}" -- "${cur}") )
      else
        COMPREPLY=( $(compgen -d -- "${cur}") )
      fi
      ;;
  esac
}

complete -F _ember_complete ember
