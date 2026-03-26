#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build-kconfig}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

CONFIG_FILE="${BUILD_DIR}/.config"
if [[ ! -f "${CONFIG_FILE}" ]]; then
  echo "Missing ${CONFIG_FILE}. Run menuconfig first." >&2
  exit 1
fi

backend=""
backend="$(awk '
  BEGIN { b="ZLOG" }
  /^CONFIG_OPENEMBER_LOG_BACKEND_SPDLOG=y/ { b="SPDLOG" }
  /^CONFIG_OPENEMBER_LOG_BACKEND_BUILTIN=y/ { b="BUILTIN" }
  END { print b }
' "${CONFIG_FILE}")"

spdlog_level="$(awk '
  BEGIN { l="info" }
  /^CONFIG_OPENEMBER_SPDLOG_LEVEL_DEBUG=y/ { l="debug" }
  /^CONFIG_OPENEMBER_SPDLOG_LEVEL_INFO=y/ { l="info" }
  /^CONFIG_OPENEMBER_SPDLOG_LEVEL_WARN=y/ { l="warn" }
  /^CONFIG_OPENEMBER_SPDLOG_LEVEL_ERROR=y/ { l="error" }
  END { print l }
' "${CONFIG_FILE}")"

spdlog_flush_level="$(awk '
  BEGIN { l="info" }
  /^CONFIG_OPENEMBER_SPDLOG_FLUSH_INFO=y/ { l="info" }
  /^CONFIG_OPENEMBER_SPDLOG_FLUSH_WARN=y/ { l="warn" }
  /^CONFIG_OPENEMBER_SPDLOG_FLUSH_ERROR=y/ { l="error" }
  END { print l }
' "${CONFIG_FILE}")"

tp_mode="$(awk '
  BEGIN { m="FETCH" }
  /^CONFIG_OPENEMBER_TP_MODE_VENDOR=y/ { m="VENDOR" }
  /^CONFIG_OPENEMBER_TP_MODE_SYSTEM=y/ { m="SYSTEM" }
  END { print m }
' "${CONFIG_FILE}")"

json_lib="$(awk '
  BEGIN { j="CJSON" }
  /^CONFIG_OPENEMBER_JSON_LIBRARY_NLOHMANN=y/ { j="NLOHMANN_JSON" }
  END { print j }
' "${CONFIG_FILE}")"

pubsub_backend="$(awk '
  BEGIN { b="ZMQ" }
  /^CONFIG_OPENEMBER_PUBSUB_BACKEND_NNG=y/ { b="NNG" }
  /^CONFIG_OPENEMBER_PUBSUB_BACKEND_LCM=y/ { b="LCM" }
  END { print b }
' "${CONFIG_FILE}")"

msgbus_backend="$(awk '
  BEGIN { b="NNG" }
  /^CONFIG_OPENEMBER_MSGBUS_BACKEND_LCM=y/ { b="LCM" }
  END { print b }
' "${CONFIG_FILE}")"

onoff() {
  local sym="$1"
  if grep -q "^${sym}=y" "${CONFIG_FILE}"; then
    echo ON
  else
    echo OFF
  fi
}

tests_enabled="$(onoff CONFIG_OPENEMBER_ENABLE_TESTS)"
examples_enabled="$(onoff CONFIG_OPENEMBER_ENABLE_EXAMPLES)"
openmp_enabled="$(onoff CONFIG_OPENEMBER_ENABLE_OPENMP)"
debug_enabled="$(onoff CONFIG_OPENEMBER_DEBUG_ENABLED)"
opt_disabled="$(onoff CONFIG_OPENEMBER_OPTIMIZATION_DISABLED)"
crosscompile_enabled="$(onoff CONFIG_OPENEMBER_CROSSCOMPILE_ENABLED)"
use_yamlcpp="$(onoff CONFIG_OPENEMBER_USE_YAMLCPP)"
use_asio="$(onoff CONFIG_OPENEMBER_USE_ASIO)"
component_network="$(onoff CONFIG_OPENEMBER_COMPONENT_NETWORK)"
module_launch_manager="$(onoff CONFIG_OPENEMBER_MODULE_LAUNCH_MANAGER)"
if ! grep -q "^CONFIG_OPENEMBER_MODULE_LAUNCH_MANAGER=" "${CONFIG_FILE}"; then
  module_launch_manager=ON
fi
module_template="$(onoff CONFIG_OPENEMBER_MODULE_TEMPLATE)"
module_alogd="$(onoff CONFIG_OPENEMBER_MODULE_ALOGD)"
module_device_manager="$(onoff CONFIG_OPENEMBER_MODULE_DEVICE_MANAGER)"
module_config_manager="$(onoff CONFIG_OPENEMBER_MODULE_CONFIG_MANAGER)"
module_monitor_alarm="$(onoff CONFIG_OPENEMBER_MODULE_MONITOR_ALARM)"
module_ota="$(onoff CONFIG_OPENEMBER_MODULE_OTA)"
module_acquisition="$(onoff CONFIG_OPENEMBER_MODULE_ACQUISITION)"
module_web_server="$(onoff CONFIG_OPENEMBER_MODULE_WEB_SERVER)"
feature_algorithm="$(onoff CONFIG_OPENEMBER_FEATURE_ALGORITHM)"
example_pubsub_two_nodes="$(onoff CONFIG_OPENEMBER_EXAMPLE_PUBSUB_TWO_NODES)"
if ! grep -q "^CONFIG_OPENEMBER_EXAMPLE_PUBSUB_TWO_NODES=" "${CONFIG_FILE}"; then
  example_pubsub_two_nodes=ON
fi
example_msgbus_nng_forwarder="$(onoff CONFIG_OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER)"
if ! grep -q "^CONFIG_OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER=" "${CONFIG_FILE}"; then
  example_msgbus_nng_forwarder=ON
fi
example_network_sockets="$(onoff CONFIG_OPENEMBER_EXAMPLE_NETWORK_SOCKETS)"
if ! grep -q "^CONFIG_OPENEMBER_EXAMPLE_NETWORK_SOCKETS=" "${CONFIG_FILE}"; then
  example_network_sockets=ON
fi

enable_osal="$(onoff CONFIG_OPENEMBER_ENABLE_OSAL)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_OSAL=" "${CONFIG_FILE}"; then
  enable_osal=ON
fi

enable_hal="$(onoff CONFIG_OPENEMBER_ENABLE_HAL)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_HAL=" "${CONFIG_FILE}"; then
  enable_hal=ON
fi

log_file="/etc/openember/zlog.conf"
log_file="$(awk '
  /^CONFIG_OPENEMBER_LOG_FILE=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_LOG_FILE=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${log_file}" ]]; then
  log_file="/etc/openember/zlog.conf"
fi

spdlog_pattern="[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v"
spdlog_pattern="$(awk '
  /^CONFIG_OPENEMBER_SPDLOG_PATTERN=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_SPDLOG_PATTERN=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${spdlog_pattern}" ]]; then
  spdlog_pattern="[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v"
fi

web_root_dir="apps/services/web_dashboard/web_root"
web_root_dir="$(awk '
  /^CONFIG_OPENEMBER_WEB_DASHBOARD_ROOT_DIR=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_WEB_DASHBOARD_ROOT_DIR=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${web_root_dir}" ]]; then
  web_root_dir="apps/services/web_dashboard/web_root"
fi

web_port="8000"
web_port="$(awk '
  /^CONFIG_OPENEMBER_WEB_DASHBOARD_PORT=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_WEB_DASHBOARD_PORT=/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${web_port}" ]]; then
  web_port="8000"
fi

out_cmake="${BUILD_DIR}/config.cmake"
cat > "${out_cmake}" <<EOF
# Auto-generated by scripts/kconfig/genconfig.sh
# Source: ${CONFIG_FILE}

set(OPENEMBER_LOG_BACKEND "${backend}" CACHE STRING "OpenEmber logging backend" FORCE)
set(OPENEMBER_LOG_FILE "${log_file}" CACHE STRING "zlog.conf path" FORCE)
set(OPENEMBER_SPDLOG_LEVEL "${spdlog_level}" CACHE STRING "spdlog log level (debug/info/warn/error)" FORCE)
set(OPENEMBER_SPDLOG_FLUSH_LEVEL "${spdlog_flush_level}" CACHE STRING "spdlog flush on level (info/warn/error)" FORCE)
set(OPENEMBER_SPDLOG_PATTERN "${spdlog_pattern}" CACHE STRING "spdlog pattern" FORCE)
set(OPENEMBER_WEB_DASHBOARD_ROOT_DIR "${web_root_dir}" CACHE STRING "web_dashboard web root directory" FORCE)
set(OPENEMBER_WEB_DASHBOARD_PORT ${web_port} CACHE STRING "web_dashboard HTTP port" FORCE)
set(OPENEMBER_THIRD_PARTY_MODE "${tp_mode}" CACHE STRING "Third-party source mode: FETCH/VENDOR/SYSTEM" FORCE)
set(OPENEMBER_JSON_LIBRARY "${json_lib}" CACHE STRING "JSON implementation for OpenEmber" FORCE)
set(OPENEMBER_WITH_YAMLCPP ${use_yamlcpp} CACHE BOOL "Fetch/use yaml-cpp (optional C++ dependency)" FORCE)
set(OPENEMBER_WITH_ASIO ${use_asio} CACHE BOOL "Fetch/use standalone Asio (optional)" FORCE)
set(OPENEMBER_COMPONENT_NETWORK ${component_network} CACHE BOOL "Build component: Network (high-level socket wrapper)" FORCE)

set(TESTS_ENABLED ${tests_enabled} CACHE BOOL "Whether to unit test" FORCE)
set(EXAMPLES_ENABLED ${examples_enabled} CACHE BOOL "Whether compile examples" FORCE)
set(OPENMP_ENABLED ${openmp_enabled} CACHE BOOL "Whether to enable omp feature" FORCE)
set(DEBUG_ENABLED ${debug_enabled} CACHE BOOL "Whether to enable debug mode" FORCE)
set(OPTIMIZATION_DISABLED ${opt_disabled} CACHE BOOL "Whether to disable optimization" FORCE)
set(CROSSCOMPILE_ENABLED ${crosscompile_enabled} CACHE BOOL "Whether to build for ARM" FORCE)

set(OPENEMBER_MODULE_LAUNCH_MANAGER ${module_launch_manager} CACHE BOOL "Build app system/launch_manager" FORCE)
set(OPENEMBER_MODULE_TEMPLATE ${module_template} CACHE BOOL "Build app examples/hello_node" FORCE)
set(OPENEMBER_MODULE_ALOGD ${module_alogd} CACHE BOOL "Build app system/log_service" FORCE)
set(OPENEMBER_MODULE_DEVICE_MANAGER ${module_device_manager} CACHE BOOL "Build app system/device_manager" FORCE)
set(OPENEMBER_MODULE_CONFIG_MANAGER ${module_config_manager} CACHE BOOL "Build app system/config_service" FORCE)
set(OPENEMBER_MODULE_MONITOR_ALARM ${module_monitor_alarm} CACHE BOOL "Build app system/health_monitor" FORCE)
set(OPENEMBER_MODULE_OTA ${module_ota} CACHE BOOL "Build app services/ota_update_service" FORCE)
set(OPENEMBER_MODULE_ACQUISITION ${module_acquisition} CACHE BOOL "Build app references/sensor_data_reference" FORCE)
set(OPENEMBER_MODULE_WEB_SERVER ${module_web_server} CACHE BOOL "Build app services/web_dashboard" FORCE)
set(OPENEMBER_FEATURE_ALGORITHM ${feature_algorithm} CACHE BOOL "Enable Algorithm module" FORCE)
set(OPENEMBER_EXAMPLE_PUBSUB_TWO_NODES ${example_pubsub_two_nodes} CACHE BOOL "Build example pubsub_two_nodes" FORCE)
set(OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER ${example_msgbus_nng_forwarder} CACHE BOOL "Build example msgbus_nng_forwarder" FORCE)
set(OPENEMBER_EXAMPLE_NETWORK_SOCKETS ${example_network_sockets} CACHE BOOL "Build example network_sockets" FORCE)

set(OPENEMBER_ENABLE_OSAL ${enable_osal} CACHE BOOL "Build platform OSAL (Linux pthread)" FORCE)
set(OPENEMBER_ENABLE_HAL ${enable_hal} CACHE BOOL "Build platform HAL (Linux file/uart; requires OSAL)" FORCE)

# Keep existing CMake backend selection interface
set(BUILD_PUBSUB_ZMQ OFF CACHE BOOL "" FORCE)
set(BUILD_PUBSUB_NNG OFF CACHE BOOL "" FORCE)
set(BUILD_PUBSUB_LCM OFF CACHE BOOL "" FORCE)
if("${pubsub_backend}" STREQUAL "ZMQ")
  set(BUILD_PUBSUB_ZMQ ON CACHE BOOL "" FORCE)
elseif("${pubsub_backend}" STREQUAL "NNG")
  set(BUILD_PUBSUB_NNG ON CACHE BOOL "" FORCE)
elseif("${pubsub_backend}" STREQUAL "LCM")
  set(BUILD_PUBSUB_LCM ON CACHE BOOL "" FORCE)
endif()

if("${msgbus_backend}" STREQUAL "LCM")
  set(OPENEMBER_MSGBUS_USE_NNG OFF CACHE BOOL "Use NNG backend for internal msgbus" FORCE)
  set(OPENEMBER_MSGBUS_USE_LCM ON CACHE BOOL "Use LCM backend for internal msgbus" FORCE)
else()
  set(OPENEMBER_MSGBUS_USE_NNG ON CACHE BOOL "Use NNG backend for internal msgbus" FORCE)
  set(OPENEMBER_MSGBUS_USE_LCM OFF CACHE BOOL "Use LCM backend for internal msgbus" FORCE)
endif()
EOF

echo "Generated: ${out_cmake}" >&2

