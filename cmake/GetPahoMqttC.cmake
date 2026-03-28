# paho.mqtt.c 依赖获取脚本（FetchContent）
#
# 参考 AimRT 的 GetXxx.cmake 组织方式：每个依赖一个脚本、用函数封装并提供稳定的变量接口。

include(FetchContent)

function(openember_get_paho_mqtt_c)
    # 目标/命名：
    # - 库目标：paho-mqtt3c / paho-mqtt3a
    # - 别名目标：paho_mqtt_c::paho-mqtt3c / paho_mqtt_c::paho-mqtt3a（可选）
    # FetchContent 工程名带版本号，生成目录如 build/_deps/openember_paho_mqtt_c_v1_3_16-src

    string(REPLACE "." "_" _paho_ver_us "${OPENEMBER_PAHO_MQTT_C_VERSION}")
    set(_fc_paho_name "openember_paho_mqtt_c_v${_paho_ver_us}")

    set(PAHO_MQTT_C_LOCAL_SOURCE "" CACHE PATH "Optional local path override for paho.mqtt.c")

    FetchContent_GetProperties(${_fc_paho_name} POPULATED _fc_paho_populated)

    if(NOT _fc_paho_populated)
        # OPENEMBER_MQTT_PAHO_TLS: Kconfig / cache — OFF 强制无 TLS；ON 强制需要 OpenSSL；未定义则尽量自动检测
        if(DEFINED OPENEMBER_MQTT_PAHO_TLS AND NOT OPENEMBER_MQTT_PAHO_TLS)
            set(PAHO_WITH_SSL OFF CACHE BOOL "Enable SSL for paho" FORCE)
            message(STATUS "Paho MQTT C: TLS disabled (OPENEMBER_MQTT_PAHO_TLS=OFF)")
        elseif(DEFINED OPENEMBER_MQTT_PAHO_TLS AND OPENEMBER_MQTT_PAHO_TLS)
            find_package(OpenSSL REQUIRED)
            set(PAHO_WITH_SSL ON CACHE BOOL "Enable SSL for paho" FORCE)
        else()
            find_package(OpenSSL QUIET)
            if(OpenSSL_FOUND)
                set(PAHO_WITH_SSL ON CACHE BOOL "Enable SSL for paho" FORCE)
            else()
                set(PAHO_WITH_SSL OFF CACHE BOOL "Enable SSL for paho" FORCE)
                message(WARNING "OpenSSL not found (install libssl-dev / openssl-devel). Paho builds without TLS; use tcp:// only or install OpenSSL for ssl:// (e.g. EMQX Cloud :8883).")
            endif()
        endif()

        if(PAHO_MQTT_C_LOCAL_SOURCE)
            FetchContent_Declare(
                ${_fc_paho_name}
                SOURCE_DIR ${PAHO_MQTT_C_LOCAL_SOURCE}
                OVERRIDE_FIND_PACKAGE
            )
        else()
            FetchContent_Declare(
                ${_fc_paho_name}
                URL ${OPENEMBER_PAHO_MQTT_C_URL}
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                OVERRIDE_FIND_PACKAGE
            )
        endif()

        # Build options（若检测到 OpenSSL 则 PAHO_WITH_SSL=ON，见上）
        set(PAHO_ENABLE_TESTING OFF CACHE BOOL "Build paho unit tests" FORCE)
        set(PAHO_ENABLE_CPACK OFF CACHE BOOL "Build paho cpack" FORCE)
        set(PAHO_BUILD_SHARED OFF CACHE BOOL "Build shared libs" FORCE)
        set(PAHO_BUILD_STATIC ON CACHE BOOL "Build static libs" FORCE)

        # Some upstreams use these options; harmless if ignored.
        set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "Disable docs" FORCE)
        set(PAHO_BUILD_SAMPLES OFF CACHE BOOL "Disable samples" FORCE)
        set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "Disable docs" FORCE)

        FetchContent_MakeAvailable(${_fc_paho_name})
    endif()

    FetchContent_GetProperties(${_fc_paho_name} SOURCE_DIR _paho_fc_source_dir)

    # Provide stable alias targets for easier future migration.
    if(TARGET paho-mqtt3cs AND NOT TARGET paho_mqtt_c::paho-mqtt3cs)
        add_library(paho_mqtt_c::paho-mqtt3cs ALIAS paho-mqtt3cs)
    endif()
    if(TARGET paho-mqtt3as AND NOT TARGET paho_mqtt_c::paho-mqtt3as)
        add_library(paho_mqtt_c::paho-mqtt3as ALIAS paho-mqtt3as)
    endif()
    if(TARGET paho-mqtt3c AND NOT TARGET paho_mqtt_c::paho-mqtt3c)
        add_library(paho_mqtt_c::paho-mqtt3c ALIAS paho-mqtt3c)
    endif()
    if(TARGET paho-mqtt3a AND NOT TARGET paho_mqtt_c::paho-mqtt3a)
        add_library(paho_mqtt_c::paho-mqtt3a ALIAS paho-mqtt3a)
    endif()

    # When building only static libraries, upstream typically creates:
    # - paho-mqtt3cs-static (OUTPUT_NAME: paho-mqtt3cs) when PAHO_WITH_SSL
    # - paho-mqtt3c-static (OUTPUT_NAME: paho-mqtt3c)
    if(TARGET paho-mqtt3cs-static AND NOT TARGET paho_mqtt_c::paho-mqtt3cs-static)
        add_library(paho_mqtt_c::paho-mqtt3cs-static ALIAS paho-mqtt3cs-static)
    endif()
    if(TARGET paho-mqtt3as-static AND NOT TARGET paho_mqtt_c::paho-mqtt3as-static)
        add_library(paho_mqtt_c::paho-mqtt3as-static ALIAS paho-mqtt3as-static)
    endif()
    if(TARGET paho-mqtt3c-static AND NOT TARGET paho_mqtt_c::paho-mqtt3c-static)
        add_library(paho_mqtt_c::paho-mqtt3c-static ALIAS paho-mqtt3c-static)
    endif()
    if(TARGET paho-mqtt3a-static AND NOT TARGET paho_mqtt_c::paho-mqtt3a-static)
        add_library(paho_mqtt_c::paho-mqtt3a-static ALIAS paho-mqtt3a-static)
    endif()

    # Legacy variable interface (给 Dependencies.cmake 的 resolve_* 返回值使用)
    set(PAHO_MQTT_C_INCLUDE_DIRS ${_paho_fc_source_dir}/src PARENT_SCOPE)

    if(DEFINED OPENEMBER_MQTT_PAHO_ASYNC AND OPENEMBER_MQTT_PAHO_ASYNC)
        set(_paho_link_async ON)
    else()
        set(_paho_link_async OFF)
    endif()

    if(_paho_link_async)
        if(TARGET paho-mqtt3as-static AND TARGET paho-mqtt3cs-static)
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3as-static paho-mqtt3cs-static PARENT_SCOPE)
        elseif(TARGET paho-mqtt3as AND TARGET paho-mqtt3cs)
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3as paho-mqtt3cs PARENT_SCOPE)
        elseif(TARGET paho-mqtt3a-static AND TARGET paho-mqtt3c-static)
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3a-static paho-mqtt3c-static PARENT_SCOPE)
        elseif(TARGET paho-mqtt3a AND TARGET paho-mqtt3c)
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3a paho-mqtt3c PARENT_SCOPE)
        else()
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3a paho-mqtt3c PARENT_SCOPE)
        endif()
    else()
        if(TARGET paho-mqtt3cs-static)
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3cs-static PARENT_SCOPE)
        elseif(TARGET paho-mqtt3cs)
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3cs PARENT_SCOPE)
        elseif(TARGET paho-mqtt3c-static)
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3c-static PARENT_SCOPE)
        elseif(TARGET paho-mqtt3c)
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3c PARENT_SCOPE)
        else()
            set(PAHO_MQTT_C_LIBRARIES paho-mqtt3c PARENT_SCOPE)
        endif()
    endif()
endfunction()

