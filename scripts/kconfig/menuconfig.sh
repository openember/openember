#!/usr/bin/env bash
# One parser/configuration path for framework and product workspaces.
set -euo pipefail

BUILD_DIR="${1:-build}"
SCRIPT_ORIGIN="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ORIGIN="$(cd "${OPENEMBER_ORIGIN:-${SCRIPT_ORIGIN}}" && pwd)"
KCONFIG_ROOT="$(cd "${OPENEMBER_CUSTOM:-${ORIGIN}}" && pwd)"
if [[ "${KCONFIG_ROOT}" != "${ORIGIN}" && -f "${KCONFIG_ROOT}/product.default" &&
      -z "${OPENEMBER_PRODUCT_MODEL:-}" ]]; then
  echo "[kconfig] Product configuration requires a model-aware ember CLI." >&2
  echo "Run from ${KCONFIG_ROOT}: ${ORIGIN}/scripts/ember menuconfig" >&2
  exit 1
fi
mkdir -p "${BUILD_DIR}"
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"
FRONTENDS="$(bash "${ORIGIN}/scripts/kconfig/ensure-kconfig-frontends-nox.sh")"
LIBDIR="${FRONTENDS}/usr/lib"
for arch in x86_64-linux-gnu aarch64-linux-gnu; do
  if [[ -d "${FRONTENDS}/usr/lib/${arch}" ]]; then
    LIBDIR="${FRONTENDS}/usr/lib/${arch}"
    break
  fi
done

# Bind all framework paths explicitly. No product file can accidentally shadow
# a nested framework Kconfig with the same relative path.
export OPENEMBER_KCONFIG_FRAMEWORK="${ORIGIN}"
for group in APPLICATIONS HARDWARE; do
  fragment="${KCONFIG_ROOT}/kconfig/${group,,}.Kconfig"
  [[ -f "${fragment}" && "${KCONFIG_ROOT}" != "${ORIGIN}" ]] ||
    fragment="${ORIGIN}/kconfig/empty.Kconfig"
  export "OPENEMBER_KCONFIG_${group}=${fragment}"
done
export OPENEMBER_PRODUCT_MODEL="${OPENEMBER_PRODUCT_MODEL:-}"
export srctree="${ORIGIN}"
export KCONFIG_AUTOCONFIG="${BUILD_DIR}/include/config/auto.conf"
export KCONFIG_AUTOHEADER="${BUILD_DIR}/include/generated/autoconf.h"

# Stage parser output so a bad Kconfig or failed TUI does not replace .config.
scratch="$(mktemp -d "${BUILD_DIR}/.kconfig.XXXXXX")"
trap 'rm -rf -- "${scratch}"' EXIT
entry="${scratch}/Kconfig"
# This frontend requires mainmenu before any symbol declarations.
sed -n '/^mainmenu /p' "${KCONFIG_ROOT}/Kconfig" > "${entry}"
if [[ -n "${OPENEMBER_PRODUCT_MODEL}" ]]; then
  sed -i "s/\"$/ - ${OPENEMBER_PRODUCT_MODEL}\"/" "${entry}"
fi
sed '/^mainmenu /d' "${KCONFIG_ROOT}/Kconfig" > "${scratch}/project.Kconfig"
for key in FRAMEWORK APPLICATIONS HARDWARE; do
  printf 'config OPENEMBER_KCONFIG_%s\n    string\n    option env="OPENEMBER_KCONFIG_%s"\n\n' "${key}" "${key}"
done >> "${entry}"
printf 'config OPENEMBER_KCONFIG_RESOLVED\n    bool\n    default y\n\n' >> "${entry}"
printf 'config PRODUCT_MODEL\n    string\n    default "%s"\n\n' "${OPENEMBER_PRODUCT_MODEL}" >> "${entry}"
entry_path="${scratch}/project.Kconfig"
entry_path="${entry_path//\\/\\\\}"
entry_path="${entry_path//\"/\\\"}"
printf 'source "%s"\n' "${entry_path}" >> "${entry}"
export KCONFIG_CONFIG="${scratch}/.config"
cd "${KCONFIG_ROOT}"
run_conf() {
  LD_LIBRARY_PATH="${LIBDIR}:${LD_LIBRARY_PATH:-}" \
    "${FRONTENDS}/usr/bin/kconfig-conf" "$@" "${entry}"
}

fragments=()
if [[ -n "${OPENEMBER_PRODUCT_MODEL}" ]]; then
  [[ ! -f "${KCONFIG_ROOT}/openember.defconfig" ]] ||
    fragments+=("${KCONFIG_ROOT}/openember.defconfig")
  fragments+=("${KCONFIG_ROOT}/products/${OPENEMBER_PRODUCT_MODEL}/product.defconfig")
fi
fingerprint=""
if [[ "${#fragments[@]}" -gt 0 ]]; then
  fingerprint="$(sha256sum "${fragments[@]}" | sha256sum | cut -d' ' -f1)"
fi
if [[ -f "${BUILD_DIR}/.config" && "${OPENEMBER_RESET_CONFIG:-0}" != 1 ]]; then
  cp "${BUILD_DIR}/.config" "${KCONFIG_CONFIG}"
  run_conf --olddefconfig >/dev/null
  if [[ -f "${BUILD_DIR}/.defconfig.sha256" &&
        "$(cat "${BUILD_DIR}/.defconfig.sha256")" != "${fingerprint}" ]]; then
    echo "[kconfig] defconfig changed; keeping your .config. Use ember defconfig to reset explicitly." >&2
  fi
else
  if [[ "${#fragments[@]}" -gt 0 ]]; then
    # Last fragment wins; let Kconfig resolve defaults, ranges and dependencies.
    awk '
      /^CONFIG_[A-Za-z0-9_]+=/ { key=$0; sub(/=.*/, "", key); values[key]=$0; next }
      /^# CONFIG_[A-Za-z0-9_]+ is not set$/ {
        key=$0; sub(/^# /, "", key); sub(/ is not set$/, "", key); values[key]=$0
      }
      END { for (key in values) print values[key] }
    ' "${fragments[@]}" > "${scratch}/defconfig"
    run_conf --defconfig="${scratch}/defconfig" >/dev/null
  else
    run_conf --alldefconfig >/dev/null
  fi
fi

if [[ "${OPENEMBER_SAVE_DEFCONFIG:-0}" == 1 ]]; then
  [[ -n "${OPENEMBER_PRODUCT_MODEL}" ]] || { echo "savedefconfig requires a product model" >&2; exit 1; }
  # Compare resolved choices with the shared baseline, not just Kconfig defaults:
  # a model may deliberately undo a value set in openember.defconfig.
  cp "${KCONFIG_CONFIG}" "${scratch}/current.config"
  export KCONFIG_CONFIG="${scratch}/baseline.config"
  export KCONFIG_AUTOCONFIG="${scratch}/baseline.auto.conf"
  export KCONFIG_AUTOHEADER="${scratch}/baseline.h"
  if [[ -f "${KCONFIG_ROOT}/openember.defconfig" ]]; then
    run_conf --defconfig="${KCONFIG_ROOT}/openember.defconfig" >/dev/null
  else
    run_conf --alldefconfig >/dev/null
  fi
  python3 "${ORIGIN}/scripts/kconfig/save-defconfig.py" "${scratch}/current.config" \
    "${scratch}/baseline.config" "${OPENEMBER_PRODUCT_MODEL}" \
    "${KCONFIG_ROOT}/products/${OPENEMBER_PRODUCT_MODEL}/product.defconfig"
  sha256sum "${fragments[@]}" | sha256sum | cut -d' ' -f1 > "${BUILD_DIR}/.defconfig.sha256"
  exit 0
fi

echo "[kconfig] product=${OPENEMBER_PRODUCT_MODEL:-framework} config=${BUILD_DIR}/.config" >&2
if [[ "${OPENEMBER_KCONFIG_NONINTERACTIVE:-0}" != 1 ]]; then
  LD_LIBRARY_PATH="${LIBDIR}:${LD_LIBRARY_PATH:-}" \
    "${FRONTENDS}/usr/bin/kconfig-mconf" "${entry}"
fi
test -s "${KCONFIG_CONFIG}"
if [[ "${OPENEMBER_RESET_CONFIG:-0}" == 1 && -f "${BUILD_DIR}/.config" ]]; then
  cp "${BUILD_DIR}/.config" "${BUILD_DIR}/.config.pre-defconfig"
fi
if ! cmp -s "${KCONFIG_CONFIG}" "${BUILD_DIR}/.config"; then
  mv "${KCONFIG_CONFIG}" "${BUILD_DIR}/.config"
fi
if [[ ! -f "${BUILD_DIR}/.defconfig.sha256" || "${OPENEMBER_RESET_CONFIG:-0}" == 1 ]]; then
  printf '%s\n' "${fingerprint}" > "${BUILD_DIR}/.defconfig.sha256"
fi
echo "Config generated: ${BUILD_DIR}/.config" >&2
