#!/usr/bin/env bash
# Exercise the actual Kconfig parser, including nested framework sources.
set -euo pipefail
framework="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "${scratch}"' EXIT

bash "${framework}/scripts/ember" new config-smoke --path "${scratch}"
for project in "${framework}" "${scratch}/config-smoke" "$@"; do
  project="$(cd "${project}" && pwd)"
  output="${scratch}/build-$(basename "${project}")"
  model=""
  [[ ! -f "${project}/product.default" ]] || read -r model < "${project}/product.default"
  OPENEMBER_ORIGIN="${framework}" OPENEMBER_CUSTOM="${project}" \
    OPENEMBER_PRODUCT_MODEL="${model}" OPENEMBER_KCONFIG_NONINTERACTIVE=1 \
    bash "${framework}/scripts/kconfig/menuconfig.sh" "${output}"
  bash "${framework}/scripts/kconfig/genconfig.sh" "${output}"
  test -s "${output}/.config"
  test -s "${output}/config.cmake"
  # Defined by the nested examples/references/Kconfig file.
  grep -q 'CONFIG_OPENEMBER_REFERENCE_SENSOR_DATA' "${output}/.config"
done
grep -q 'CONFIG_PRODUCT_BUILD_APP=y' "${scratch}/build-config-smoke/.config"

# An old installed CLI must not silently edit a different build/.config.
if OPENEMBER_ORIGIN="${framework}" OPENEMBER_CUSTOM="${scratch}/config-smoke" \
  OPENEMBER_PRODUCT_MODEL= OPENEMBER_KCONFIG_NONINTERACTIVE=1 \
  bash "${framework}/scripts/kconfig/menuconfig.sh" "${scratch}/legacy-build" \
  > "${scratch}/legacy.log" 2>&1; then
  echo 'ERROR: legacy product entry without a model was accepted' >&2; exit 1
fi
grep -q 'model-aware ember CLI' "${scratch}/legacy.log"
test ! -e "${scratch}/legacy-build/.config"

# A parser error must propagate without replacing an existing configuration
# with defaults. Use the interactive path, where the old fallback lived.
broken="${scratch}/broken"
mkdir -p "${broken}" "${scratch}/broken-build"
printf 'source "missing-Kconfig"\n' > "${broken}/Kconfig"
printf '# existing user configuration\n' > "${scratch}/broken-build/.config"
cp "${scratch}/broken-build/.config" "${scratch}/original.config"
if TERM=xterm OPENEMBER_ORIGIN="${framework}" OPENEMBER_CUSTOM="${broken}" \
  OPENEMBER_KCONFIG_NONINTERACTIVE=0 bash "${framework}/scripts/kconfig/menuconfig.sh" \
  "${scratch}/broken-build" > "${scratch}/failure.log" 2>&1; then
  echo 'ERROR: invalid Kconfig unexpectedly succeeded' >&2
  exit 1
fi
cmp "${scratch}/original.config" "${scratch}/broken-build/.config"
if grep -q 'fallback' "${scratch}/failure.log"; then
  echo 'ERROR: menuconfig failure triggered fallback' >&2
  exit 1
fi
echo 'Product Kconfig regression checks passed.'
