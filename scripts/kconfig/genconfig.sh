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
  BEGIN { b="SPDLOG" }
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
enable_link="$(onoff CONFIG_OPENEMBER_ENABLE_LINK)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_LINK=" "${CONFIG_FILE}"; then
  enable_link=ON
fi
enable_msgs="$(onoff CONFIG_OPENEMBER_ENABLE_MSGS)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_MSGS=" "${CONFIG_FILE}"; then
  enable_msgs=ON
fi
msgs_ref="$(awk '
  BEGIN { r="main" }
  /^CONFIG_OPENEMBER_MSGS_REF_LATEST=y/ { r="main" }
  END { print r }
' "${CONFIG_FILE}")"
msgs_source="$(awk '
  BEGIN { s="FETCH" }
  /^CONFIG_OPENEMBER_MSGS_SOURCE_LOCAL=y/ { s="LOCAL" }
  /^CONFIG_OPENEMBER_MSGS_SOURCE_FETCH=y/ { s="FETCH" }
  END { print s }
' "${CONFIG_FILE}")"
msgs_local_source="$(awk '
  /^CONFIG_OPENEMBER_MSGS_LOCAL_SOURCE=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MSGS_LOCAL_SOURCE=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
use_asio="$(onoff CONFIG_OPENEMBER_USE_ASIO)"
use_ruckig="$(onoff CONFIG_OPENEMBER_USE_RUCKIG)"
if ! grep -q "^CONFIG_OPENEMBER_USE_RUCKIG=" "${CONFIG_FILE}"; then
  use_ruckig=OFF
fi
component_network="$(onoff CONFIG_OPENEMBER_COMPONENT_NETWORK)"
component_transport="$(onoff CONFIG_OPENEMBER_COMPONENT_TRANSPORT)"
if ! grep -q "^CONFIG_OPENEMBER_COMPONENT_TRANSPORT=" "${CONFIG_FILE}"; then
  component_transport=ON
fi
if [[ "${component_transport}" == "OFF" ]]; then
  enable_link=OFF
fi
component_mqtt="$(onoff CONFIG_OPENEMBER_COMPONENT_MQTT)"
if ! grep -q "^CONFIG_OPENEMBER_COMPONENT_MQTT=" "${CONFIG_FILE}"; then
  component_mqtt=ON
fi
mqtt_paho_tls="$(onoff CONFIG_OPENEMBER_MQTT_PAHO_TLS)"
if ! grep -q "^CONFIG_OPENEMBER_MQTT_PAHO_TLS=" "${CONFIG_FILE}"; then
  mqtt_paho_tls=ON
fi
mqtt_paho_async="$(onoff CONFIG_OPENEMBER_MQTT_PAHO_ASYNC)"
if ! grep -q "^CONFIG_OPENEMBER_MQTT_PAHO_ASYNC=" "${CONFIG_FILE}"; then
  mqtt_paho_async=OFF
fi
system_launch_manager="$(onoff CONFIG_OPENEMBER_SYSTEM_LAUNCH_MANAGER)"
example_hello_node="$(onoff CONFIG_OPENEMBER_EXAMPLE_HELLO_NODE)"
system_log_service="$(onoff CONFIG_OPENEMBER_SYSTEM_LOG_SERVICE)"
system_device_manager="$(onoff CONFIG_OPENEMBER_SYSTEM_DEVICE_MANAGER)"
system_config_service="$(onoff CONFIG_OPENEMBER_SYSTEM_CONFIG_SERVICE)"
system_health_monitor="$(onoff CONFIG_OPENEMBER_SYSTEM_HEALTH_MONITOR)"
service_ota_update="$(onoff CONFIG_OPENEMBER_SERVICE_OTA_AGENT)"
reference_sensor_data="$(onoff CONFIG_OPENEMBER_REFERENCE_SENSOR_DATA)"
service_web_console="$(onoff CONFIG_OPENEMBER_SERVICE_WEB_CONSOLE)"
service_logger="$(onoff CONFIG_OPENEMBER_SERVICE_LOGGER)"
component_algorithm="$(onoff CONFIG_OPENEMBER_COMPONENT_ALGORITHM)"
if ! grep -q "^CONFIG_OPENEMBER_COMPONENT_ALGORITHM=" "${CONFIG_FILE}"; then
  component_algorithm=ON
fi
component_thread_pool="$(onoff CONFIG_OPENEMBER_COMPONENT_THREAD_POOL)"
if ! grep -q "^CONFIG_OPENEMBER_COMPONENT_THREAD_POOL=" "${CONFIG_FILE}"; then
  component_thread_pool=ON
fi
example_network_sockets="$(onoff CONFIG_OPENEMBER_EXAMPLE_NETWORK_SOCKETS)"
example_transport="$(onoff CONFIG_OPENEMBER_EXAMPLE_TRANSPORT)"
if ! grep -q "^CONFIG_OPENEMBER_EXAMPLE_TRANSPORT=" "${CONFIG_FILE}"; then
  example_transport=ON
fi
# Core 与 Link 绑定：Link 开启时构建 openember::core
if [[ "${enable_link}" == "ON" ]]; then
  enable_core=ON
else
  enable_core=OFF
fi
example_core="$(onoff CONFIG_OPENEMBER_EXAMPLE_CORE)"
if ! grep -q "^CONFIG_OPENEMBER_EXAMPLE_CORE=" "${CONFIG_FILE}"; then
  example_core=ON
fi
if [[ "${enable_link}" == "OFF" ]] || [[ "${examples_enabled}" == "OFF" ]]; then
  example_core=OFF
fi
if ! grep -q "^CONFIG_OPENEMBER_EXAMPLE_NETWORK_SOCKETS=" "${CONFIG_FILE}"; then
  example_network_sockets=ON
fi

example_mqtt_emqx="$(onoff CONFIG_OPENEMBER_EXAMPLE_MQTT_EMQX)"
if [[ "${component_mqtt}" == "OFF" ]]; then
  example_mqtt_emqx=OFF
fi

enable_osal="$(onoff CONFIG_OPENEMBER_ENABLE_OSAL)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_OSAL=" "${CONFIG_FILE}"; then
  enable_osal=ON
fi

enable_lpio="$(onoff CONFIG_OPENEMBER_ENABLE_LPIO)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_LPIO=" "${CONFIG_FILE}"; then
  enable_lpio=ON
fi

enable_lpio_examples="$(onoff CONFIG_OPENEMBER_ENABLE_LPIO_EXAMPLES)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_LPIO_EXAMPLES=" "${CONFIG_FILE}"; then
  enable_lpio_examples=OFF
fi

enable_tools="$(onoff CONFIG_OPENEMBER_ENABLE_TOOLS)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_TOOLS=" "${CONFIG_FILE}"; then
  enable_tools=ON
fi

enable_tools_emcom="$(onoff CONFIG_OPENEMBER_ENABLE_TOOLS_EMCOM)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_TOOLS_EMCOM=" "${CONFIG_FILE}"; then
  enable_tools_emcom=ON
fi

enable_tools_sbus_receiver="$(onoff CONFIG_OPENEMBER_ENABLE_TOOLS_SBUS_RECEIVER)"
if ! grep -q "^CONFIG_OPENEMBER_ENABLE_TOOLS_SBUS_RECEIVER=" "${CONFIG_FILE}"; then
  enable_tools_sbus_receiver=ON
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

web_root_dir="services/web_console/web_root"
web_root_dir="$(awk '
  /^CONFIG_OPENEMBER_WEB_CONSOLE_ROOT_DIR=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_WEB_CONSOLE_ROOT_DIR=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${web_root_dir}" ]]; then
  web_root_dir="services/web_console/web_root"
fi

web_port="8000"
web_port="$(awk '
  /^CONFIG_OPENEMBER_WEB_CONSOLE_PORT=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_WEB_CONSOLE_PORT=/,"",v)
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

mqtt_emqx_broker_uri_override="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_BROKER_URI=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_BROKER_URI=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"

mqtt_emqx_host="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_HOST=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_HOST=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${mqtt_emqx_host}" ]]; then
  mqtt_emqx_host="broker.emqx.io"
fi

mqtt_emqx_port="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_PORT=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_PORT=/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${mqtt_emqx_port}" ]]; then
  if grep -q "^CONFIG_OPENEMBER_MQTT_EMQX_TRANSPORT_WSS=y" "${CONFIG_FILE}"; then
    mqtt_emqx_port=8084
  else
    mqtt_emqx_port=8883
  fi
fi

mqtt_emqx_wss_path="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_WSS_PATH=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_WSS_PATH=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${mqtt_emqx_wss_path}" ]]; then
  mqtt_emqx_wss_path="/mqtt"
fi

if [[ -n "${mqtt_emqx_broker_uri_override}" ]]; then
  mqtt_emqx_broker_uri="${mqtt_emqx_broker_uri_override}"
elif grep -q "^CONFIG_OPENEMBER_MQTT_EMQX_TRANSPORT_WSS=y" "${CONFIG_FILE}"; then
  mqtt_emqx_broker_uri="wss://${mqtt_emqx_host}:${mqtt_emqx_port}${mqtt_emqx_wss_path}"
else
  mqtt_emqx_broker_uri="ssl://${mqtt_emqx_host}:${mqtt_emqx_port}"
fi

mqtt_emqx_client_id="openember-mqtt-example"
mqtt_emqx_client_id="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_CLIENT_ID=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_CLIENT_ID=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${mqtt_emqx_client_id}" ]]; then
  mqtt_emqx_client_id="$(awk '
    /^CONFIG_OPENEMBER_MQTT_CLIENT_ID=/ {
      v=$0
      sub(/^CONFIG_OPENEMBER_MQTT_CLIENT_ID=/,"",v)
      gsub(/^"/,"",v); gsub(/"$/,"",v)
      print v
      exit
    }
  ' "${CONFIG_FILE}")"
fi
if [[ -z "${mqtt_emqx_client_id}" ]]; then
  mqtt_emqx_client_id="openember-mqtt-example"
fi

mqtt_emqx_username="emqx"
mqtt_emqx_username="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_USERNAME=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_USERNAME=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${mqtt_emqx_username}" ]]; then
  mqtt_emqx_username="$(awk '
    /^CONFIG_OPENEMBER_MQTT_USERNAME=/ {
      v=$0
      sub(/^CONFIG_OPENEMBER_MQTT_USERNAME=/,"",v)
      gsub(/^"/,"",v); gsub(/"$/,"",v)
      print v
      exit
    }
  ' "${CONFIG_FILE}")"
fi
if [[ -z "${mqtt_emqx_username}" ]]; then
  mqtt_emqx_username="emqx"
fi

mqtt_emqx_password="public"
mqtt_emqx_password="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_PASSWORD=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_PASSWORD=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${mqtt_emqx_password}" ]]; then
  mqtt_emqx_password="$(awk '
    /^CONFIG_OPENEMBER_MQTT_PASSWORD=/ {
      v=$0
      sub(/^CONFIG_OPENEMBER_MQTT_PASSWORD=/,"",v)
      gsub(/^"/,"",v); gsub(/"$/,"",v)
      print v
      exit
    }
  ' "${CONFIG_FILE}")"
fi
if [[ -z "${mqtt_emqx_password}" ]]; then
  mqtt_emqx_password="public"
fi

mqtt_emqx_topic="emqx/c-test"
mqtt_emqx_topic="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_TOPIC=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_TOPIC=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${mqtt_emqx_topic}" ]]; then
  mqtt_emqx_topic="$(awk '
    /^CONFIG_OPENEMBER_MQTT_TOPIC=/ {
      v=$0
      sub(/^CONFIG_OPENEMBER_MQTT_TOPIC=/,"",v)
      gsub(/^"/,"",v); gsub(/"$/,"",v)
      print v
      exit
    }
  ' "${CONFIG_FILE}")"
fi
if [[ -z "${mqtt_emqx_topic}" ]]; then
  mqtt_emqx_topic="emqx/c-test"
fi

mqtt_emqx_ssl_cafile=""
mqtt_emqx_ssl_cafile="$(awk '
  /^CONFIG_OPENEMBER_MQTT_EMQX_SSL_CAFILE=/ {
    v=$0
    sub(/^CONFIG_OPENEMBER_MQTT_EMQX_SSL_CAFILE=/,"",v)
    gsub(/^"/,"",v); gsub(/"$/,"",v)
    print v
    exit
  }
' "${CONFIG_FILE}")"
if [[ -z "${mqtt_emqx_ssl_cafile}" ]]; then
  mqtt_emqx_ssl_cafile="$(awk '
    /^CONFIG_OPENEMBER_MQTT_SSL_CAFILE=/ {
      v=$0
      sub(/^CONFIG_OPENEMBER_MQTT_SSL_CAFILE=/,"",v)
      gsub(/^"/,"",v); gsub(/"$/,"",v)
      print v
      exit
    }
  ' "${CONFIG_FILE}")"
fi

mqtt_emqx_ssl_verify=ON
if grep -q "^CONFIG_OPENEMBER_MQTT_EMQX_SSL_VERIFY=" "${CONFIG_FILE}"; then
  mqtt_emqx_ssl_verify="$(onoff CONFIG_OPENEMBER_MQTT_EMQX_SSL_VERIFY)"
elif grep -q "^CONFIG_OPENEMBER_MQTT_SSL_VERIFY=" "${CONFIG_FILE}"; then
  mqtt_emqx_ssl_verify="$(onoff CONFIG_OPENEMBER_MQTT_SSL_VERIFY)"
else
  mqtt_emqx_ssl_verify=ON
fi

# Third-party bundle toggles (no versions here; see cmake/Dependencies.cmake)
kconfig_bundle_on() {
  local sym="$1"
  if grep -q "^${sym}=y" "${CONFIG_FILE}"; then
    echo ON
  elif grep -q "^# ${sym} is not set" "${CONFIG_FILE}"; then
    echo OFF
  elif grep -q "^${sym}=n" "${CONFIG_FILE}"; then
    echo OFF
  else
    echo ON
  fi
}

bundle_spdlog="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_SPDLOG)"
bundle_nlohmann="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_NLOHMANN_JSON)"
bundle_sqlite="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_SQLITE)"
bundle_mongoose="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_MONGOOSE)"
bundle_yamlcpp="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_YAMLCPP)"
bundle_asio="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_ASIO)"
bundle_paho="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_PAHO)"
bundle_nng="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_NNG)"
bundle_lcm="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_LCM)"
bundle_libzmq="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_LIBZMQ)"
bundle_cppzmq="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_CPPZMQ)"
bundle_zenoh_c="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_ZENOH_C)"
bundle_zenohcxx="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_ZENOHCXX)"
bundle_ruckig="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_RUCKIG)"
bundle_openember_msgs="$(kconfig_bundle_on CONFIG_OPENEMBER_THIRD_PARTY_BUNDLE_OPENEMBER_MSGS)"

out_cmake="${BUILD_DIR}/config.cmake"
cat > "${out_cmake}" <<EOF
# Auto-generated by scripts/kconfig/genconfig.sh
# Source: ${CONFIG_FILE}

set(OPENEMBER_LOG_BACKEND "${backend}" CACHE STRING "OpenEmber logging backend" FORCE)
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
set(OPENEMBER_SPDLOG_TOPIC_LEVEL "${spdlog_topic_level}" CACHE STRING "spdlog topic publish level threshold" FORCE)
set(OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT ${spdlog_topic_rate_limit} CACHE STRING "spdlog topic rate limit (lines/sec)" FORCE)
set(OPENEMBER_WEB_CONSOLE_ROOT_DIR "${web_root_dir}" CACHE STRING "web_console web root directory" FORCE)
set(OPENEMBER_WEB_CONSOLE_PORT ${web_port} CACHE STRING "web_console HTTP port" FORCE)
set(OPENEMBER_LOGGER_PORT ${logger_port} CACHE STRING "logger HTTP port" FORCE)
set(OPENEMBER_LOGGER_LOG_DIR "${logger_log_dir}" CACHE STRING "logger source log directory" FORCE)
set(OPENEMBER_THIRD_PARTY_MODE "${tp_mode}" CACHE STRING "Third-party source mode: FETCH/VENDOR/SYSTEM" FORCE)
set(OPENEMBER_WITH_YAMLCPP ${use_yamlcpp} CACHE BOOL "Fetch/use yaml-cpp (optional C++ dependency)" FORCE)
set(OPENEMBER_ENABLE_LINK ${enable_link} CACHE BOOL "Build OpenEmber Link communication layer" FORCE)
set(OPENEMBER_ENABLE_MSGS ${enable_msgs} CACHE BOOL "Build openember-msgs C++ protocol bindings" FORCE)
set(OPENEMBER_MSGS_SOURCE "${msgs_source}" CACHE STRING "openember-msgs source: FETCH or LOCAL" FORCE)
set_property(CACHE OPENEMBER_MSGS_SOURCE PROPERTY STRINGS FETCH LOCAL)
set(OPENEMBER_MSGS_REF "${msgs_ref}" CACHE STRING "openember-msgs git ref" FORCE)
set_property(CACHE OPENEMBER_MSGS_REF PROPERTY STRINGS main)
set(OPENEMBER_MSGS_LOCAL_SOURCE "${msgs_local_source}" CACHE PATH "Absolute path to local openember-msgs checkout" FORCE)
set(OPENEMBER_WITH_ASIO ${use_asio} CACHE BOOL "Fetch/use standalone Asio (optional)" FORCE)
set(OPENEMBER_COMPONENT_NETWORK ${component_network} CACHE BOOL "Build component: Network (high-level socket wrapper)" FORCE)
set(OPENEMBER_COMPONENT_TRANSPORT ${component_transport} CACHE BOOL "Build component: Transport (Zenoh)" FORCE)
set(OPENEMBER_ENABLE_CORE ${enable_core} CACHE BOOL "Build C++ core (Node/Topic/Service)" FORCE)
set(OPENEMBER_COMPONENT_MQTT ${component_mqtt} CACHE BOOL "Build component: MQTT (Paho C)" FORCE)
set(OPENEMBER_MQTT_PAHO_TLS ${mqtt_paho_tls} CACHE BOOL "Build Paho MQTT C with OpenSSL (TLS)" FORCE)
set(OPENEMBER_MQTT_PAHO_ASYNC ${mqtt_paho_async} CACHE BOOL "Link Paho MQTTAsync library" FORCE)

set(TESTS_ENABLED ${tests_enabled} CACHE BOOL "Whether to unit test" FORCE)
set(EXAMPLES_ENABLED ${examples_enabled} CACHE BOOL "Whether compile examples" FORCE)
set(OPENMP_ENABLED ${openmp_enabled} CACHE BOOL "Whether to enable omp feature" FORCE)
set(DEBUG_ENABLED ${debug_enabled} CACHE BOOL "Whether to enable debug mode" FORCE)
set(OPTIMIZATION_DISABLED ${opt_disabled} CACHE BOOL "Whether to disable optimization" FORCE)
set(CROSSCOMPILE_ENABLED ${crosscompile_enabled} CACHE BOOL "Whether to build for ARM" FORCE)

set(OPENEMBER_SYSTEM_LAUNCH_MANAGER ${system_launch_manager} CACHE BOOL "Build system/launch_manager" FORCE)
set(OPENEMBER_EXAMPLE_HELLO_NODE ${example_hello_node} CACHE BOOL "Build examples/hello_node" FORCE)
set(OPENEMBER_SYSTEM_LOG_SERVICE ${system_log_service} CACHE BOOL "Build system/log_service" FORCE)
set(OPENEMBER_SYSTEM_DEVICE_MANAGER ${system_device_manager} CACHE BOOL "Build system/device_manager" FORCE)
set(OPENEMBER_SYSTEM_CONFIG_SERVICE ${system_config_service} CACHE BOOL "Build system/config_service" FORCE)
set(OPENEMBER_SYSTEM_HEALTH_MONITOR ${system_health_monitor} CACHE BOOL "Build system/health_monitor" FORCE)
set(OPENEMBER_SERVICE_OTA_AGENT ${service_ota_update} CACHE BOOL "Build services/ota_agent" FORCE)
set(OPENEMBER_REFERENCE_SENSOR_DATA ${reference_sensor_data} CACHE BOOL "Build examples/references/sensor_data_reference" FORCE)
set(OPENEMBER_SERVICE_WEB_CONSOLE ${service_web_console} CACHE BOOL "Build services/web_console" FORCE)
set(OPENEMBER_SERVICE_LOGGER ${service_logger} CACHE BOOL "Build services/logger" FORCE)
set(OPENEMBER_COMPONENT_ALGORITHM ${component_algorithm} CACHE BOOL "Build Algorithm component" FORCE)
set(OPENEMBER_COMPONENT_THREAD_POOL ${component_thread_pool} CACHE BOOL "Build Thread Pool component" FORCE)
set(OPENEMBER_EXAMPLE_NETWORK_SOCKETS ${example_network_sockets} CACHE BOOL "Build example network_sockets" FORCE)
set(OPENEMBER_EXAMPLE_TRANSPORT ${example_transport} CACHE BOOL "Build examples transport_talker/listener" FORCE)
set(OPENEMBER_EXAMPLE_CORE ${example_core} CACHE BOOL "Build examples openember_topic_* / openember_service_*" FORCE)
set(OPENEMBER_EXAMPLE_MQTT_EMQX ${example_mqtt_emqx} CACHE BOOL "Build example mqtt_emqx_client" FORCE)
set(OPENEMBER_MQTT_EMQX_BROKER_URI "${mqtt_emqx_broker_uri}" CACHE STRING "MQTT broker URI for mqtt_emqx example" FORCE)
set(OPENEMBER_MQTT_EMQX_CLIENT_ID "${mqtt_emqx_client_id}" CACHE STRING "MQTT client id for mqtt_emqx example" FORCE)
set(OPENEMBER_MQTT_EMQX_USERNAME "${mqtt_emqx_username}" CACHE STRING "MQTT username for mqtt_emqx example" FORCE)
set(OPENEMBER_MQTT_EMQX_PASSWORD "${mqtt_emqx_password}" CACHE STRING "MQTT password for mqtt_emqx example" FORCE)
set(OPENEMBER_MQTT_EMQX_TOPIC "${mqtt_emqx_topic}" CACHE STRING "MQTT topic for mqtt_emqx example" FORCE)
set(OPENEMBER_MQTT_EMQX_SSL_CAFILE "${mqtt_emqx_ssl_cafile}" CACHE STRING "MQTT TLS CA PEM path for mqtt_emqx example" FORCE)
set(OPENEMBER_MQTT_EMQX_SSL_VERIFY ${mqtt_emqx_ssl_verify} CACHE BOOL "Verify MQTT TLS server cert for mqtt_emqx example" FORCE)

set(OPENEMBER_ENABLE_OSAL ${enable_osal} CACHE BOOL "Build platform OSAL (Linux pthread)" FORCE)
set(OPENEMBER_ENABLE_LPIO ${enable_lpio} CACHE BOOL "Build platform LPIO (C++ Linux peripherals)" FORCE)
set(OPENEMBER_ENABLE_LPIO_EXAMPLES ${enable_lpio_examples} CACHE BOOL "Build platform LPIO examples" FORCE)
set(OPENEMBER_ENABLE_TOOLS ${enable_tools} CACHE BOOL "Build OpenEmber utilities (tools/)" FORCE)
set(OPENEMBER_ENABLE_TOOLS_EMCOM ${enable_tools_emcom} CACHE BOOL "Build emcom serial console tool" FORCE)
set(OPENEMBER_ENABLE_TOOLS_SBUS_RECEIVER ${enable_tools_sbus_receiver} CACHE BOOL "Build sbus-receiver SBUS monitor tool" FORCE)

set(OPENEMBER_THIRD_PARTY_BUNDLE_SPDLOG ${bundle_spdlog} CACHE BOOL "Third-party bundle: spdlog" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_NLOHMANN_JSON ${bundle_nlohmann} CACHE BOOL "Third-party bundle: nlohmann/json" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_SQLITE ${bundle_sqlite} CACHE BOOL "Third-party bundle: SQLite" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_MONGOOSE ${bundle_mongoose} CACHE BOOL "Third-party bundle: Mongoose" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_YAMLCPP ${bundle_yamlcpp} CACHE BOOL "Third-party bundle: yaml-cpp" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_ASIO ${bundle_asio} CACHE BOOL "Third-party bundle: Asio" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_PAHO ${bundle_paho} CACHE BOOL "Third-party bundle: Paho MQTT C" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_NNG ${bundle_nng} CACHE BOOL "Third-party bundle: NNG" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_LCM ${bundle_lcm} CACHE BOOL "Third-party bundle: LCM" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_LIBZMQ ${bundle_libzmq} CACHE BOOL "Third-party bundle: libzmq" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_CPPZMQ ${bundle_cppzmq} CACHE BOOL "Third-party bundle: cppzmq" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_ZENOH_C ${bundle_zenoh_c} CACHE BOOL "Third-party bundle: zenoh-c" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_ZENOHCXX ${bundle_zenohcxx} CACHE BOOL "Third-party bundle: zenoh-cpp" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_RUCKIG ${bundle_ruckig} CACHE BOOL "Third-party bundle: ruckig" FORCE)
set(OPENEMBER_THIRD_PARTY_BUNDLE_OPENEMBER_MSGS ${bundle_openember_msgs} CACHE BOOL "Third-party bundle: openember-msgs" FORCE)
EOF

echo "Generated: ${out_cmake}" >&2
