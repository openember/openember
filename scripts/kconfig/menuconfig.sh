#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build-kconfig}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
KCONFIG_FRONTENDS_DIR="$(bash "${ROOT_DIR}/scripts/kconfig/ensure-kconfig-frontends-nox.sh")"

mkdir -p "${BUILD_DIR}"
for kf in "${ROOT_DIR}"/Kconfig*; do
  if [[ -f "${kf}" ]]; then
    cp -f "${kf}" "${BUILD_DIR}/$(basename "${kf}")"
  fi
done

# kconfig-frontends-nox 提供了 kconfig-mconf/c f 等可执行文件，这些可执行文件依赖动态库，
# 需要通过 LD_LIBRARY_PATH 指定到解压目录的 lib。
LIBDIR=""
if [[ -d "${KCONFIG_FRONTENDS_DIR}/usr/lib/x86_64-linux-gnu" ]]; then
  LIBDIR="${KCONFIG_FRONTENDS_DIR}/usr/lib/x86_64-linux-gnu"
elif [[ -d "${KCONFIG_FRONTENDS_DIR}/usr/lib/aarch64-linux-gnu" ]]; then
  LIBDIR="${KCONFIG_FRONTENDS_DIR}/usr/lib/aarch64-linux-gnu"
else
  LIBDIR="${KCONFIG_FRONTENDS_DIR}/usr/lib"
fi

cd "${BUILD_DIR}"

echo "Launching menuconfig in: ${BUILD_DIR}" >&2
echo "Log backend choices: OPENEMBER_LOG_BACKEND_*" >&2

# 非交互模式：直接生成默认配置（适合 CI / 无 TTY）
if [[ "${OPENEMBER_KCONFIG_NONINTERACTIVE:-0}" == "1" ]]; then
  LD_LIBRARY_PATH="${LIBDIR}:${LD_LIBRARY_PATH:-}" \
    "${KCONFIG_FRONTENDS_DIR}/usr/bin/kconfig-conf" --alldefconfig "Kconfig" >/dev/null
else
  # 尝试交互式 menuconfig（mconf），失败则回退到默认配置
  if ! LD_LIBRARY_PATH="${LIBDIR}:${LD_LIBRARY_PATH:-}" \
    "${KCONFIG_FRONTENDS_DIR}/usr/bin/kconfig-mconf" "Kconfig" ; then
    echo "mconf failed, fallback to --alldefconfig" >&2
    LD_LIBRARY_PATH="${LIBDIR}:${LD_LIBRARY_PATH:-}" \
      "${KCONFIG_FRONTENDS_DIR}/usr/bin/kconfig-conf" --alldefconfig "Kconfig" >/dev/null
  fi
fi

echo "Config generated: ${BUILD_DIR}/.config" >&2

