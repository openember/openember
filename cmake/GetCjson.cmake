# cJSON 依赖获取脚本（FetchContent），参考 AimRT 的 GetXxx.cmake 组织方式

include(FetchContent)

function(openember_get_cjson)
    # Targets/names used by OpenEmber:
    # - library target: `cjson`
    # - alias target:   `cJSON::cJSON`

    set(OPENEMBER_CJSON_LOCAL_SOURCE "" CACHE PATH "Optional local path override for cJSON")

    # Declare first, then make available (确保目标一定进入主构建目标列表)。
    if(OPENEMBER_CJSON_LOCAL_SOURCE)
        FetchContent_Declare(
            openember_cjson
            SOURCE_DIR ${OPENEMBER_CJSON_LOCAL_SOURCE}
            OVERRIDE_FIND_PACKAGE
        )
    else()
        FetchContent_Declare(
            openember_cjson
            URL ${OPENEMBER_CJSON_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
    endif()

    # cJSON build is already minimal; disable optional extras by default.
    set(ENABLE_CJSON_UTILS OFF CACHE BOOL "Build cJSON_Utils" FORCE)
    # cJSON tests 会创建同名 unity target，和其它依赖（例如 libzmq tests）
    # 在同一工程内配置时可能发生 CMP0002 目标冲突。
    set(ENABLE_CJSON_TEST OFF CACHE BOOL "Build cJSON test targets" FORCE)

    FetchContent_MakeAvailable(openember_cjson)

    if(TARGET cjson AND NOT TARGET cJSON::cJSON)
        add_library(cJSON::cJSON ALIAS cjson)
    endif()

    # Keep legacy variable interface for the rest of OpenEmber.
    set(CJSON_INCLUDE_DIRS ${openember_cjson_SOURCE_DIR} PARENT_SCOPE)
    if(TARGET cjson)
        set(CJSON_LIBRARIES cjson PARENT_SCOPE)
    else()
        set(CJSON_LIBRARIES cjson PARENT_SCOPE)
    endif()
endfunction()

