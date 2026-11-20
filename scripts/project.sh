# Shared workspace selection for ember menuconfig/update/build/run.
# Sourced by scripts/ember.

project_error() { echo "[ember] $*" >&2; exit 1; }

select_workspace() {
  ROOT_DIR="$(resolve_realpath "$(find_project_root)")"
  export_project_roots "${ROOT_DIR}"
  MODEL="${PRODUCT:-}"
  EMBER_CLI_JOBS=""
  local explicit_model=0 requested_dir="" active_model="" active_dir="" cache_root=""
  RUN_ARGS=()
  DRY_RUN=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -j|--jobs)
        [[ $# -ge 2 ]] || project_error "$1 requires a positive integer"
        ember_jobs_option "$2"; shift 2 ;;
      -j?*) ember_jobs_option "${1#-j}"; shift ;;
      --jobs=*) ember_jobs_option "${1#--jobs=}"; shift ;;
      --product)
        [[ $# -ge 2 && -n "$2" ]] || project_error "--product requires a model"
        MODEL="$2"; explicit_model=1; shift 2 ;;
      --build-dir)
        [[ $# -ge 2 && -n "$2" ]] || project_error "--build-dir requires a directory"
        requested_dir="$2"; shift 2 ;;
      --dry-run)
        [[ "${cmd}" == run ]] || project_error "--dry-run is only supported by run"
        DRY_RUN=1; shift ;;
      --)
        [[ "${cmd}" == run ]] || project_error "Unexpected --"
        shift; RUN_ARGS=("$@"); break ;;
      -*) project_error "Unknown option: $1" ;;
      *)
        [[ -z "${requested_dir}" ]] || project_error "Only one build directory is allowed"
        requested_dir="$1"; shift ;;
    esac
  done
  ACTIVE_FILE="${ROOT_DIR}/build/.ember-active"
  if [[ -n "${OPENEMBER_CUSTOM:-}" ]]; then
    if [[ -f "${ACTIVE_FILE}" ]]; then
      { read -r active_model; read -r active_dir || true; } < "${ACTIVE_FILE}"
    fi
    # An explicit legacy build path carries its model, if already configured.
    local path_model=""
    if [[ -n "${requested_dir:-${OPENEMBER_BUILD_DIR:-}}" ]]; then
      local candidate="${requested_dir:-${OPENEMBER_BUILD_DIR}}"
      [[ "${candidate}" == /* ]] || candidate="${ROOT_DIR}/${candidate}"
      if [[ -f "${candidate}/.product-model" ]]; then
        path_model="$(cat "${candidate}/.product-model")"
      elif [[ -f "${candidate}/.config" ]]; then
        path_model="$(sed -n 's/^CONFIG_PRODUCT_MODEL="\(.*\)"$/\1/p' "${candidate}/.config")"
      fi
    fi
    MODEL="${MODEL:-${path_model:-${active_model}}}"
    if [[ -z "${MODEL}" && -f "${ROOT_DIR}/product.default" ]]; then
      read -r MODEL < "${ROOT_DIR}/product.default"
    fi
    [[ "${MODEL}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] ||
      project_error "Select a model with --product <name> (or add product.default)"
    [[ -f "${ROOT_DIR}/products/${MODEL}/product.defconfig" ]] ||
      project_error "Unknown product model: ${MODEL}"
    bd="${requested_dir:-${OPENEMBER_BUILD_DIR:-}}"
    if [[ -z "${bd}" ]]; then
      if [[ "${active_model}" == "${MODEL}" && -n "${active_dir}" && "${explicit_model}" == 0 ]]; then
        bd="${active_dir}"
      else
        bd="${ROOT_DIR}/build/${MODEL}"
      fi
    fi
  else
    [[ -z "${MODEL}" ]] || project_error "--product requires a product workspace"
    bd="${requested_dir:-${OPENEMBER_BUILD_DIR:-build}}"
  fi
  [[ "${bd}" == /* ]] || bd="${ROOT_DIR}/${bd}"
  bd="$(realpath -m "${bd}")"
  # Refuse a build path that contains the source tree.
  [[ "${ROOT_DIR}/" != "${bd%/}/"* ]] ||
    project_error "Build directory cannot be the project root or its parent: ${bd}"
  if [[ -f "${bd}/.project-root" && "$(cat "${bd}/.project-root")" != "${ROOT_DIR}" ]]; then
    project_error "Build directory belongs to another workspace: ${bd}"
  fi
  if [[ -f "${bd}/CMakeCache.txt" ]]; then
    cache_root="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "${bd}/CMakeCache.txt")"
    [[ "$(resolve_realpath "${cache_root}")" == "${ROOT_DIR}" ]] ||
      project_error "Legacy/different CMake source in ${bd}; use a new --build-dir"
  fi
  if [[ -n "${MODEL}" ]]; then
    local saved_model=""
    if [[ -f "${bd}/.product-model" ]]; then
      saved_model="$(cat "${bd}/.product-model")"
    elif [[ -f "${bd}/.config" ]]; then
      saved_model="$(sed -n 's/^CONFIG_PRODUCT_MODEL="\(.*\)"$/\1/p' "${bd}/.config")"
    fi
    [[ -z "${saved_model}" || "${saved_model}" == "${MODEL}" ]] ||
      project_error "${bd} belongs to ${saved_model}, not ${MODEL}; use separate directories"
  fi
  export OPENEMBER_PRODUCT_MODEL="${MODEL}"
  if [[ "${cmd}" == build || "${cmd}" == all ]]; then
    ember_select_build_jobs
  fi
  echo "[ember] model=${MODEL:-framework} build=${bd}" >&2
}

remember_workspace() {
  mkdir -p "${bd}"
  printf '%s\n' "${ROOT_DIR}" > "${bd}/.project-root"
  if [[ -n "${MODEL}" ]]; then
    printf '%s\n' "${MODEL}" > "${bd}/.product-model"
    mkdir -p "$(dirname "${ACTIVE_FILE}")"
    local active_tmp
    active_tmp="$(mktemp "${ACTIVE_FILE}.XXXXXX")"
    printf '%s\n%s\n' "${MODEL}" "${bd}" > "${active_tmp}"
    mv "${active_tmp}" "${ACTIVE_FILE}"
  fi
}

prepare_config() {
  OPENEMBER_KCONFIG_NONINTERACTIVE=1 bash "${OPENEMBER_ORIGIN}/scripts/kconfig/menuconfig.sh" "${bd}"
  bash "${OPENEMBER_ORIGIN}/scripts/kconfig/genconfig.sh" "${bd}"
}

configure_workspace() {
  # A failed reconfigure/build must not leave a previous success marker valid.
  rm -f -- "${bd}/.built-config.sha256"
  local args=()
  if [[ -n "${MODEL}" ]]; then
    args+=("-DOPENEMBER_PRODUCT_MODEL=${MODEL}" "-DOPENEMBER_DIR=${OPENEMBER_ORIGIN}")
  fi
  local local_config="${OPENEMBER_CMAKE_LOCAL:-${ROOT_DIR}/.ember-local.cmake}"
  if [[ -n "${OPENEMBER_CMAKE_LOCAL:-}" || -f "${local_config}" ]]; then
    [[ "${local_config}" == /* ]] || local_config="${ROOT_DIR}/${local_config}"
    [[ -f "${local_config}" ]] || project_error "Local CMake configuration missing: ${local_config}"
    echo "[ember] local build environment: ${local_config}" >&2
    args=("-C" "${local_config}" "${args[@]}")
  fi
  cmake -S "${ROOT_DIR}" -B "${bd}" "${args[@]}"
}

build_workspace() {
  # Build every configured target, including framework services selected in
  # menuconfig; restricting this to app dependencies would ignore those edits.
  cmake --build "${bd}" -j"${JOBS}"
  sha256sum "${bd}/.config" | cut -d' ' -f1 > "${bd}/.built-config.sha256"
}

run_workspace() {
  [[ -n "${MODEL}" ]] || project_error "run requires a product workspace with products/<model>/launch.yaml"
  local launch="${ROOT_DIR}/products/${MODEL}/launch.yaml"
  [[ -f "${launch}" ]] || project_error "Launch file missing: ${launch}"
  local command=("${bd}/bin/launch_manager" --launch "${launch}")
  if [[ "${DRY_RUN}" == 1 ]]; then
    printf '%q ' "${command[@]}" "${RUN_ARGS[@]}"; printf '\n'
    return
  fi
  [[ -x "${bd}/bin/launch_manager" ]] ||
    project_error "Build this model with launch_manager enabled: ember build --product ${MODEL}"
  grep -q '^CONFIG_OPENEMBER_SYSTEM_LAUNCH_MANAGER=y$' "${bd}/.config" ||
    project_error "launch_manager is disabled in this model; enable it in menuconfig and rebuild"
  [[ -f "${bd}/.built-config.sha256" && -f "${bd}/.config" ]] ||
    project_error "Run ember build for this model before ember run"
  [[ "$(sha256sum "${bd}/.config" | cut -d' ' -f1)" == "$(cat "${bd}/.built-config.sha256")" ]] ||
    project_error "Configuration changed since the last successful build; run ember build"
  remember_workspace
  cd "${ROOT_DIR}"
  exec "${command[@]}" "${RUN_ARGS[@]}"
}
