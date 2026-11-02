#!/usr/bin/env bash
set -euo pipefail
framework="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "${scratch}"' EXIT
unset OPENEMBER_ROOT OPENEMBER_BUILD_DIR OPENEMBER_DIR PRODUCT OPENEMBER_CUSTOM
unset OPENEMBER_JOBS CARGO_BUILD_JOBS CMAKE_BUILD_PARALLEL_LEVEL
export OPENEMBER_KCONFIG_NONINTERACTIVE=1
cli="${framework}/scripts/ember"
bash "${cli}" new workflow --path "${scratch}" --model host
cd "${scratch}/workflow"
cp "${framework}/scripts/kconfig/fixtures/"* .
cp -r products/host products/small
printf '\nCONFIG_PRODUCT_BUILD_APP=n\n' >> products/small/product.defconfig

bash "${cli}" menuconfig
test -s build/host/.config
grep -q 'CONFIG_PRODUCT_MODEL="host"' build/host/.config
! test -e build/.config
# Menu edits survive build and regenerate stale config.cmake.
sed -i 's/CONFIG_OPENEMBER_USE_ASIO=y/# CONFIG_OPENEMBER_USE_ASIO is not set/' build/host/.config
OPENEMBER_JOBS=6 CARGO_BUILD_JOBS=1 bash "${cli}" build -j 2
test -x build/host/framework_service
grep -q 'set(OPENEMBER_WITH_ASIO OFF' build/host/config.cmake
bash "${cli}" run > "${scratch}/host-run"
grep -Fq "/products/host/launch.yaml" "${scratch}/host-run"

bash "${cli}" menuconfig --product small
bash "${cli}" build
grep -q 'PRODUCT_BUILD_APP:BOOL=OFF' build/small/CMakeCache.txt
test ! -e build/small/product_app
bash "${cli}" run > "${scratch}/small-run"
grep -Fq "/products/small/launch.yaml" "${scratch}/small-run"
grep -q 'CONFIG_PRODUCT_MODEL="host"' build/host/.config
grep -q 'CONFIG_PRODUCT_MODEL="small"' build/small/.config

expect_failure() {
  if "$@" > "${scratch}/error.log" 2>&1; then
    echo "Expected failure: $*" >&2; exit 1
  fi
}
# Failed builds with unchanged configuration invalidate the previous success.
expect_failure env OPENEMBER_TEST_FAIL_BUILD=1 bash "${cli}" build
test ! -f build/small/.built-config.sha256
expect_failure bash "${cli}" run
bash "${cli}" build
# Changed config must not silently launch stale binaries.
printf '\n# menu edit\n' >> build/small/.config
expect_failure bash "${cli}" run
grep -q 'Configuration changed' "${scratch}/error.log"
expect_failure bash "${cli}" menuconfig --product host --build-dir build/small
expect_failure bash "${cli}" menuconfig --product ../escape
expect_failure bash "${cli}" clean --build-dir .
# Fragment updates preserve user edits until an explicit reset.
printf '\nCONFIG_PRODUCT_BUILD_APP=y\n' >> products/small/product.defconfig
bash "${cli}" genconfig
grep -q 'set(PRODUCT_BUILD_APP OFF' build/small/config.cmake
bash "${cli}" defconfig
test -s build/small/.config.pre-defconfig
grep -q 'CONFIG_PRODUCT_BUILD_APP=y' build/small/.config
# Selecting a custom path stays consistent with the next bare command.
bash "${cli}" menuconfig --product host --build-dir 'build/custom path'
bash "${cli}" build
test -x 'build/custom path/bin/launch_manager'
bash "${cli}" run > "${scratch}/custom-run"
grep -Fq "/products/host/launch.yaml" "${scratch}/custom-run"
# Product-relative entry resolution also works from a subdirectory.
cd apps
OPENEMBER_DIR="${framework}" bash "${cli}" run --dry-run | grep -Fq 'custom\ path/bin/launch_manager'
cd ..
# Kconfig dependencies also turn product apps off when Link is disabled.
sed -i 's/CONFIG_OPENEMBER_ENABLE_LINK=y/# CONFIG_OPENEMBER_ENABLE_LINK is not set/' 'build/custom path/.config'
bash "${cli}" genconfig
grep -q 'set(OPENEMBER_ENABLE_LINK OFF' 'build/custom path/config.cmake'
OPENEMBER_JOBS=2 CMAKE_BUILD_PARALLEL_LEVEL=2 CARGO_BUILD_JOBS=2 bash "${cli}" update
! grep -q 'PRODUCT_BUILD_APP:BOOL=ON' 'build/custom path/CMakeCache.txt'
grep -q 'set(OPENEMBER_SYSTEM_LAUNCH_MANAGER OFF' 'build/custom path/config.cmake'
expect_failure bash "${cli}" new invalid --path "${scratch}" --model ../escape
# Completion discovers model names from within a product subdirectory.
source "${framework}/scripts/completions/ember.bash"
cd apps
COMP_WORDS=(ember menuconfig --product sm)
COMP_CWORD=3
_ember_complete
[[ "${COMPREPLY[*]}" == small ]]
echo 'Workspace CLI regression checks passed.'
