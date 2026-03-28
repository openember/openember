# cJSON：third_party/ 缓存 + build/_deps/cJSON-<ver>/

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_get_cjson)
    set(ENABLE_CJSON_UTILS OFF CACHE BOOL "Build cJSON_Utils" FORCE)
    set(ENABLE_CJSON_TEST OFF CACHE BOOL "Build cJSON test targets" FORCE)

    if(OPENEMBER_CJSON_LOCAL_SOURCE)
        set(_src "${OPENEMBER_CJSON_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_CJSON_CACHE_KEY}" "${OPENEMBER_CJSON_STAGE_DIR_NAME}"
            "${OPENEMBER_CJSON_URL}" "CMakeLists.txt" "")
    endif()

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_CJSON_STAGE_DIR_NAME}-build")

    if(TARGET cjson AND NOT TARGET cJSON::cJSON)
        add_library(cJSON::cJSON ALIAS cjson)
    endif()

    set(CJSON_INCLUDE_DIRS ${_src} PARENT_SCOPE)
    set(CJSON_LIBRARIES cjson PARENT_SCOPE)
endfunction()
