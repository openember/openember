# Build concurrency policy shared by the bundled and standalone ember CLI.
# Keep scripts/parallel.sh identical in openember and openember-cli.

ember_jobs_error() { echo "[ember] $*" >&2; return 1; }

ember_validate_jobs() {
  # Reject zero (which some build tools interpret as unlimited), signs and
  # expressions. Bound the value to the range accepted by CMake's -j option.
  [[ "$1" =~ ^[1-9][0-9]*$ && ${#1} -le 10 ]] &&
    (( 10#$1 <= 2147483647 ))
}

ember_jobs_option() {
  [[ "${cmd}" == build || "${cmd}" == all ]] ||
    { ember_jobs_error "-j/--jobs is only supported by build and all"; return 1; }
  ember_validate_jobs "$1" ||
    { ember_jobs_error "-j/--jobs requires a positive integer (1..2147483647)"; return 1; }
  EMBER_CLI_JOBS="$1"
}

ember_default_jobs() {
  local cpu_count
  cpu_count="$(nproc 2>/dev/null)" || cpu_count=""
  if ! ember_validate_jobs "${cpu_count}"; then
    cpu_count="$(getconf _NPROCESSORS_ONLN 2>/dev/null)" || cpu_count=""
  fi
  ember_validate_jobs "${cpu_count}" || cpu_count=1
  local jobs=$(( cpu_count / 2 ))
  (( jobs >= 1 )) || jobs=1
  (( jobs <= 8 )) || jobs=8
  printf '%s\n' "${jobs}"
}

ember_select_build_jobs() {
  local source=default
  if [[ -n "${EMBER_CLI_JOBS:-}" ]]; then
    JOBS="${EMBER_CLI_JOBS}"
    source=command-line
  elif [[ -n "${OPENEMBER_JOBS:-}" ]]; then
    JOBS="${OPENEMBER_JOBS}"
    source=OPENEMBER_JOBS
  else
    JOBS="$(ember_default_jobs)"
  fi
  ember_validate_jobs "${JOBS}" ||
    { ember_jobs_error "OPENEMBER_JOBS must be a positive integer (1..2147483647)"; return 1; }
  local cargo_jobs="${CARGO_BUILD_JOBS:-${JOBS}}"
  ember_validate_jobs "${cargo_jobs}" ||
    { ember_jobs_error "CARGO_BUILD_JOBS must be a positive integer (1..2147483647)"; return 1; }

  # Inherit into configure-time builds and nested Cargo/CMake processes too.
  # These are per-tool job limits, not a global CPU/threads quota.
  export CMAKE_BUILD_PARALLEL_LEVEL="${JOBS}"
  export CARGO_BUILD_JOBS="${cargo_jobs}"
  echo "[ember] build jobs=${JOBS} (${source}); cargo jobs=${cargo_jobs}" >&2
}
