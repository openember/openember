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

spdlog_topic_level="$(awk '
  BEGIN { l="info" }
  /^CONFIG_OPENEMBER_SPDLOG_TOPIC_LEVEL_DEBUG=y/ { l="debug" }
  /^CONFIG_OPENEMBER_SPDLOG_TOPIC_LEVEL_INFO=y/ { l="info" }
  /^CONFIG_OPENEMBER_SPDLOG_TOPIC_LEVEL_WARN=y/ { l="warn" }
  /^CONFIG_OPENEMBER_SPDLOG_TOPIC_LEVEL_ERROR=y/ { l="error" }
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

msgbus_backend="$(awk '
  BEGIN { b="LCM" }
  /^CONFIG_OPENEMBER_MSGBUS_BACKEND_UDP=y/ { b="UDP" }
  /^CONFIG_OPENEMBER_MSGBUS_BACKEND_ZMQ=y/ { b="ZMQ" }
  /^CONFIG_OPENEMBER_MSGBUS_BACKEND_NNG=y/ { b="NNG" }
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
module_template="$(onoff CONFIG_OPENEMBER_MODULE_TEMPLATE)"
module_alogd="$(onoff CONFIG_OPENEMBER_MODULE_ALOGD)"
module_device_manager="$(onoff CONFIG_OPENEMBER_MODULE_DEVICE_MANAGER)"
module_config_manager="$(onoff CONFIG_OPENEMBER_MODULE_CONFIG_MANAGER)"
module_monitor_alarm="$(onoff CONFIG_OPENEMBER_MODULE_MONITOR_ALARM)"
module_ota="$(onoff CONFIG_OPENEMBER_MODULE_OTA)"
module_acquisition="$(onoff CONFIG_OPENEMBER_MODULE_ACQUISITION)"
module_web_server="$(onoff CONFIG_OPENEMBER_MODULE_WEB_SERVER)"
module_logger="$(onoff CONFIG_OPENEMBER_MODULE_LOGGER)"
feature_algorithm="$(onoff CONFIG_OPENEMBER_FEATURE_ALGORITHM)"
example_msgbus_two_nodes="$(onoff CONFIG_OPENEMBER_EXAMPLE_MSGBUS_TWO_NODES)"
example_msgbus_nng_forwarder="$(onoff CONFIG_OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER)"
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

spdlog_stdout="$(onoff CONFIG_OPENEMBER_SPDLOG_ENABLE_STDOUT)"
spdlog_file="$(onoff CONFIG_OPENEMBER_SPDLOG_ENABLE_FILE)"
spdlog_syslog="$(onoff CONFIG_OPENEMBER_SPDLOG_ENABLE_SYSLOG)"
spdlog_topic="$(onoff CONFIG_OPENEMBER_SPDLOG_ENABLE_TOPIC)"

spdlog_file_dir="/var/log/openember"
spdlog_file_dir="$(awk '
  /^CONFIG_OPENEMBER_SPDLOG_FILE_DIR=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_SPDLOG_FILE_DIR=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${spdlog_file_dir}" ]]; then
  spdlog_file_dir="/var/log/openember"
fi

spdlog_rotate_max_mb="10"
spdlog_rotate_max_mb="$(awk '
  /^CONFIG_OPENEMBER_SPDLOG_ROTATE_MAX_MB=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_SPDLOG_ROTATE_MAX_MB=/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${spdlog_rotate_max_mb}" ]]; then
  spdlog_rotate_max_mb="10"
fi

spdlog_rotate_max_files="5"
spdlog_rotate_max_files="$(awk '
  /^CONFIG_OPENEMBER_SPDLOG_ROTATE_MAX_FILES=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_SPDLOG_ROTATE_MAX_FILES=/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${spdlog_rotate_max_files}" ]]; then
  spdlog_rotate_max_files="5"
fi

spdlog_topic_name="/openember/log"
spdlog_topic_name="$(awk '
  /^CONFIG_OPENEMBER_SPDLOG_TOPIC_NAME=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_SPDLOG_TOPIC_NAME=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${spdlog_topic_name}" ]]; then
  spdlog_topic_name="/openember/log"
fi

spdlog_topic_pub_url="tcp://*:7561"
spdlog_topic_pub_url="$(awk '
  /^CONFIG_OPENEMBER_SPDLOG_TOPIC_PUB_URL=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_SPDLOG_TOPIC_PUB_URL=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${spdlog_topic_pub_url}" ]]; then
  if [[ "${msgbus_backend}" == "LCM" ]]; then
    spdlog_topic_pub_url="udpm://239.255.76.67:7667?ttl=1"
  else
    spdlog_topic_pub_url="tcp://*:7561"
  fi
fi

spdlog_topic_sub_url="tcp://127.0.0.1:7561"
spdlog_topic_sub_url="$(awk '
  /^CONFIG_OPENEMBER_SPDLOG_TOPIC_SUB_URL=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_SPDLOG_TOPIC_SUB_URL=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${spdlog_topic_sub_url}" ]]; then
  if [[ "${msgbus_backend}" == "LCM" ]]; then
    spdlog_topic_sub_url="udpm://239.255.76.67:7667?ttl=1"
  else
    spdlog_topic_sub_url="tcp://127.0.0.1:7561"
  fi
fi

spdlog_topic_rate_limit="0"
spdlog_topic_rate_limit="$(awk '
  /^CONFIG_OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT=/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${spdlog_topic_rate_limit}" ]]; then
  spdlog_topic_rate_limit="0"
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

logger_port="18081"
logger_port="$(awk '
  /^CONFIG_OPENEMBER_LOGGER_PORT=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_LOGGER_PORT=/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${logger_port}" ]]; then
  logger_port="18081"
fi

logger_log_dir="/var/log/openember"
logger_log_dir="$(awk '
  /^CONFIG_OPENEMBER_LOGGER_LOG_DIR=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_LOGGER_LOG_DIR=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${logger_log_dir}" ]]; then
  logger_log_dir="/var/log/openember"
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
set(OPENEMBER_SPDLOG_ENABLE_STDOUT ${spdlog_stdout} CACHE BOOL "Enable spdlog stdout sink" FORCE)
set(OPENEMBER_SPDLOG_ENABLE_FILE ${spdlog_file} CACHE BOOL "Enable spdlog rotating file sink" FORCE)
set(OPENEMBER_SPDLOG_FILE_DIR "${spdlog_file_dir}" CACHE STRING "spdlog file directory" FORCE)
set(OPENEMBER_SPDLOG_ROTATE_MAX_MB ${spdlog_rotate_max_mb} CACHE STRING "spdlog rotate max MiB" FORCE)
set(OPENEMBER_SPDLOG_ROTATE_MAX_FILES ${spdlog_rotate_max_files} CACHE STRING "spdlog rotate max files" FORCE)
set(OPENEMBER_SPDLOG_ENABLE_SYSLOG ${spdlog_syslog} CACHE BOOL "Enable spdlog syslog sink" FORCE)
set(OPENEMBER_SPDLOG_ENABLE_TOPIC ${spdlog_topic} CACHE BOOL "Enable spdlog topic sink" FORCE)
set(OPENEMBER_SPDLOG_TOPIC_NAME "${spdlog_topic_name}" CACHE STRING "spdlog log topic name" FORCE)
set(OPENEMBER_SPDLOG_TOPIC_PUB_URL "${spdlog_topic_pub_url}" CACHE STRING "spdlog topic publisher URL" FORCE)
set(OPENEMBER_SPDLOG_TOPIC_SUB_URL "${spdlog_topic_sub_url}" CACHE STRING "spdlog topic subscriber URL" FORCE)
set(OPENEMBER_SPDLOG_TOPIC_LEVEL "${spdlog_topic_level}" CACHE STRING "spdlog topic publish level threshold" FORCE)
set(OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT ${spdlog_topic_rate_limit} CACHE STRING "spdlog topic rate limit (lines/sec)" FORCE)
set(OPENEMBER_WEB_DASHBOARD_ROOT_DIR "${web_root_dir}" CACHE STRING "web_dashboard web root directory" FORCE)
set(OPENEMBER_WEB_DASHBOARD_PORT ${web_port} CACHE STRING "web_dashboard HTTP port" FORCE)
set(OPENEMBER_LOGGER_PORT ${logger_port} CACHE STRING "logger HTTP port" FORCE)
set(OPENEMBER_LOGGER_LOG_DIR "${logger_log_dir}" CACHE STRING "logger source log directory" FORCE)
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
set(OPENEMBER_MODULE_LOGGER ${module_logger} CACHE BOOL "Build app services/logger" FORCE)
set(OPENEMBER_FEATURE_ALGORITHM ${feature_algorithm} CACHE BOOL "Enable Algorithm module" FORCE)
set(OPENEMBER_EXAMPLE_MSGBUS_TWO_NODES ${example_msgbus_two_nodes} CACHE BOOL "Build example msgbus_two_nodes" FORCE)
set(OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER ${example_msgbus_nng_forwarder} CACHE BOOL "Build example msgbus_nng_forwarder" FORCE)
set(OPENEMBER_EXAMPLE_NETWORK_SOCKETS ${example_network_sockets} CACHE BOOL "Build example network_sockets" FORCE)

set(OPENEMBER_ENABLE_OSAL ${enable_osal} CACHE BOOL "Build platform OSAL (Linux pthread)" FORCE)
set(OPENEMBER_ENABLE_HAL ${enable_hal} CACHE BOOL "Build platform HAL (Linux file/uart; requires OSAL)" FORCE)

if("${msgbus_backend}" STREQUAL "LCM")
  set(OPENEMBER_MSGBUS_USE_NNG OFF CACHE BOOL "Use NNG backend for internal msgbus" FORCE)
  set(OPENEMBER_MSGBUS_USE_LCM ON CACHE BOOL "Use LCM backend for internal msgbus" FORCE)
else()
  set(OPENEMBER_MSGBUS_USE_NNG ON CACHE BOOL "Use NNG backend for internal msgbus" FORCE)
  set(OPENEMBER_MSGBUS_USE_LCM OFF CACHE BOOL "Use LCM backend for internal msgbus" FORCE)
endif()
EOF

echo "Generated: ${out_cmake}" >&2

