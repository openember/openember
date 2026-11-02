#!/usr/bin/env bash
# Test CLI argument handling with a fake CMake: no downloads or heavy builds.
# Optional arguments are additional ember CLI paths (e.g. openember-cli).
set -euo pipefail
framework="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
clis=("${framework}/scripts/ember")
for cli in "$@"; do
  clis+=("$(realpath "${cli}")")
done
scratch="$(mktemp -d)"
trap 'rm -rf -- "${scratch}"' EXIT
unset OPENEMBER_ROOT OPENEMBER_CUSTOM OPENEMBER_DIR OPENEMBER_BUILD_DIR PRODUCT
unset OPENEMBER_JOBS CARGO_BUILD_JOBS CMAKE_BUILD_PARALLEL_LEVEL EMBER_CLI_JOBS
mkdir -p "${scratch}/bin" "${scratch}/project/scripts/kconfig" "${scratch}/project/build path"
cp "${framework}/scripts/tests/parallel/"{cmake,nproc} "${scratch}/bin/"
chmod +x "${scratch}/bin/"*
export PATH="${scratch}/bin:${PATH}"
export EMBER_PARALLEL_TRACE="${scratch}/trace"
export OPENEMBER_KCONFIG_NONINTERACTIVE=1
cp "${framework}/scripts/tests/parallel/noop" "${scratch}/project/scripts/kconfig/menuconfig.sh"
cp "${framework}/scripts/tests/parallel/noop" "${scratch}/project/scripts/kconfig/genconfig.sh"
touch "${scratch}/project/CMakeLists.txt" "${scratch}/project/build path/config.cmake"
printf 'CONFIG_PARALLEL_TEST=y\n' > "${scratch}/project/build path/.config"
cd "${scratch}/project"

expect_jobs() {
  local jobs="$1" cargo_jobs="$2"
  shift 2
  : > "${EMBER_PARALLEL_TRACE}"
  "$@" > "${scratch}/output" 2>&1
  [[ "$(grep -c "^cmake_jobs=${jobs} cargo_jobs=${cargo_jobs}$" "${EMBER_PARALLEL_TRACE}")" == 2 ]]
  grep -Fxq "arg=-j${jobs}" "${EMBER_PARALLEL_TRACE}"
  grep -Fq "build jobs=${jobs}" "${scratch}/output"
}

expect_invalid() {
  : > "${EMBER_PARALLEL_TRACE}"
  if "$@" > "${scratch}/error" 2>&1; then
    echo "Expected failure: $*" >&2; exit 1
  fi
  test ! -s "${EMBER_PARALLEL_TRACE}"
  grep -q 'positive integer' "${scratch}/error"
}

for cli in "${clis[@]}"; do
  source "$(dirname "${cli}")/parallel.sh"
  # Core discovery failure falls back safely; odd core counts round down.
  (
    nproc() { return 1; }
    getconf() { printf '6\n'; }
    [[ "$(ember_default_jobs)" == 3 ]]
    getconf() { return 1; }
    [[ "$(ember_default_jobs)" == 1 ]]
  )
  for pair in 1:1 2:1 3:1 4:2 8:4 16:8 32:8 128:8; do
    export EMBER_TEST_CPUS="${pair%:*}"
    jobs="${pair#*:}"
    expect_jobs "${jobs}" "${jobs}" bash "${cli}" build 'build path'
  done
  expect_jobs 3 3 env OPENEMBER_JOBS=3 bash "${cli}" build 'build path'
  expect_jobs 24 24 env OPENEMBER_JOBS=24 bash "${cli}" build 'build path'
  expect_jobs 4 4 env OPENEMBER_JOBS=12 bash "${cli}" build -j 4 'build path'
  expect_jobs 5 5 env OPENEMBER_JOBS=invalid bash "${cli}" build 'build path' -j5
  expect_jobs 6 6 bash "${cli}" build --jobs 6 'build path'
  expect_jobs 7 7 bash "${cli}" build 'build path' --jobs=7
  expect_jobs 2 2 env CMAKE_BUILD_PARALLEL_LEVEL=32 bash "${cli}" all 'build path' -j2
  expect_jobs 4 2 env CARGO_BUILD_JOBS=2 bash "${cli}" build --jobs=4 'build path'
  for invalid in 0 -1 abc 1.5 1+2 2147483648 99999999999999999999; do
    expect_invalid bash "${cli}" build 'build path' --jobs="${invalid}"
    expect_invalid env OPENEMBER_JOBS="${invalid}" bash "${cli}" build 'build path'
  done
  expect_invalid bash "${cli}" build 'build path' -j
  expect_invalid bash "${cli}" build 'build path' --jobs=
  expect_invalid env CARGO_BUILD_JOBS=0 bash "${cli}" build 'build path'
  # Both option names and numeric values participate in Bash completion.
  source "$(dirname "${cli}")/completions/ember.bash"
  COMP_WORDS=(ember build --j); COMP_CWORD=2
  _ember_complete
  [[ "${COMPREPLY[*]}" == --jobs ]]
  COMP_WORDS=(ember all -j 4); COMP_CWORD=3
  _ember_complete
  [[ "${COMPREPLY[*]}" == 4 ]]
  echo "Parallelism checks passed: ${cli}"
done
