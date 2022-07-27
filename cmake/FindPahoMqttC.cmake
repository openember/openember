# find the Paho MQTT C library
message(STATUS "Find the paho.mqtt.c library")

if(PAHO_WITH_SSL)
    set(_PAHO_MQTT_C_LIB_3A_NAME  paho-mqtt3as)
    set(_PAHO_MQTT_C_LIB_3C_NAME  paho-mqtt3cs)
    find_package(OpenSSL REQUIRED)
else()
    set(_PAHO_MQTT_C_LIB_3A_NAME  paho-mqtt3a)
    set(_PAHO_MQTT_C_LIB_3C_NAME  paho-mqtt3c)
endif()

# add suffix when using static Paho MQTT C library variant on Windows
if(WIN32)
    if(PAHO_BUILD_STATIC)
        set(_PAHO_MQTT_C_LIB_3A_NAME ${_PAHO_MQTT_C_LIB_3A_NAME}-static)
        set(_PAHO_MQTT_C_LIB_3C_NAME ${_PAHO_MQTT_C_LIB_3C_NAME}-static)
    endif()
endif()

find_library(PAHO_MQTT_C_3A_LIBRARY NAMES ${_PAHO_MQTT_C_LIB_3A_NAME})
find_library(PAHO_MQTT_C_3C_LIBRARY NAMES ${_PAHO_MQTT_C_LIB_3C_NAME})

unset(_PAHO_MQTT_C_LIB_3A_NAME)
unset(_PAHO_MQTT_C_LIB_3C_NAME)
find_path(PAHO_MQTT_C_INCLUDE_DIRS NAMES MQTTClient.h MQTTAsync.h)

set(PAHO_MQTT_C_LIBRARIES ${PAHO_MQTT_C_3A_LIBRARY} ${PAHO_MQTT_C_3C_LIBRARY})

add_library(PahoMqttC::PahoMqttC UNKNOWN IMPORTED)

set_target_properties(PahoMqttC::PahoMqttC PROPERTIES
    IMPORTED_LOCATION "${PAHO_MQTT_C_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${PAHO_MQTT_C_INCLUDE_DIRS}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C")
if(PAHO_WITH_SSL)
    set_target_properties(PahoMqttC::PahoMqttC PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "OPENSSL=1"
        INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto")
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PahoMqttC
    REQUIRED_VARS PAHO_MQTT_C_LIBRARIES PAHO_MQTT_C_INCLUDE_DIRS)
