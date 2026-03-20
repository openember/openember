# paho.mqtt.c 依赖获取脚本（FetchContent）
#
# 参考 AimRT 的 GetXxx.cmake 组织方式：每个依赖一个脚本、用函数封装并提供稳定的变量接口。

include(FetchContent)

function(openember_get_paho_mqtt_c)
    # 目标/命名：
    # - 库目标：paho-mqtt3c / paho-mqtt3a
    # - 别名目标：paho_mqtt_c::paho-mqtt3c / paho_mqtt_c::paho-mqtt3a（可选）

    set(PAHO_MQTT_C_LOCAL_SOURCE "" CACHE PATH "Optional local path override for paho.mqtt.c")

    FetchContent_GetProperties(openember_paho_mqtt_c)

    if(NOT openember_paho_mqtt_c_POPULATED)
        if(PAHO_MQTT_C_LOCAL_SOURCE)
            FetchContent_Declare(
                openember_paho_mqtt_c
                SOURCE_DIR ${PAHO_MQTT_C_LOCAL_SOURCE}
                OVERRIDE_FIND_PACKAGE
            )
        else()
            FetchContent_Declare(
                openember_paho_mqtt_c
                URL ${OPENEMBER_PAHO_MQTT_C_URL}
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                OVERRIDE_FIND_PACKAGE
            )
        endif()

        # Build options（默认走无 SSL 路线，减少系统依赖）
        set(PAHO_ENABLE_TESTING OFF CACHE BOOL "Build paho unit tests" FORCE)
        set(PAHO_ENABLE_CPACK OFF CACHE BOOL "Build paho cpack" FORCE)
        set(PAHO_WITH_SSL OFF CACHE BOOL "Enable SSL for paho" FORCE)
        set(PAHO_BUILD_SHARED OFF CACHE BOOL "Build shared libs" FORCE)
        set(PAHO_BUILD_STATIC ON CACHE BOOL "Build static libs" FORCE)

        # Some upstreams use these options; harmless if ignored.
        set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "Disable docs" FORCE)
        set(PAHO_BUILD_SAMPLES OFF CACHE BOOL "Disable samples" FORCE)
        set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "Disable docs" FORCE)

        FetchContent_MakeAvailable(openember_paho_mqtt_c)
    endif()

    # Provide stable alias targets for easier future migration.
    if(TARGET paho-mqtt3c AND NOT TARGET paho_mqtt_c::paho-mqtt3c)
        add_library(paho_mqtt_c::paho-mqtt3c ALIAS paho-mqtt3c)
    endif()
    if(TARGET paho-mqtt3a AND NOT TARGET paho_mqtt_c::paho-mqtt3a)
        add_library(paho_mqtt_c::paho-mqtt3a ALIAS paho-mqtt3a)
    endif()

    # When building only static libraries, upstream typically creates:
    # - paho-mqtt3c-static (OUTPUT_NAME: paho-mqtt3c)
    # - paho-mqtt3a-static (OUTPUT_NAME: paho-mqtt3a)
    if(TARGET paho-mqtt3c-static AND NOT TARGET paho_mqtt_c::paho-mqtt3c-static)
        add_library(paho_mqtt_c::paho-mqtt3c-static ALIAS paho-mqtt3c-static)
    endif()
    if(TARGET paho-mqtt3a-static AND NOT TARGET paho_mqtt_c::paho-mqtt3a-static)
        add_library(paho_mqtt_c::paho-mqtt3a-static ALIAS paho-mqtt3a-static)
    endif()

    # Legacy variable interface (给 Dependencies.cmake 的 resolve_* 返回值使用)
    set(PAHO_MQTT_C_INCLUDE_DIRS ${openember_paho_mqtt_c_SOURCE_DIR}/src PARENT_SCOPE)
    # Prefer the real CMake targets. Otherwise CMake will fall back to -lxxx
    # which may fail in FETCH build trees.
    if(TARGET paho-mqtt3a-static AND TARGET paho-mqtt3c-static)
        set(PAHO_MQTT_C_LIBRARIES paho-mqtt3a-static paho-mqtt3c-static PARENT_SCOPE)
    elseif(TARGET paho-mqtt3a AND TARGET paho-mqtt3c)
        set(PAHO_MQTT_C_LIBRARIES paho-mqtt3a paho-mqtt3c PARENT_SCOPE)
    else()
        # Fallback: keep old behavior (may require link search paths to work).
        set(PAHO_MQTT_C_LIBRARIES paho-mqtt3a paho-mqtt3c PARENT_SCOPE)
    endif()
endfunction()

