# spdlog

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_get_spdlog)
    if(OPENEMBER_SPDLOG_LOCAL_SOURCE)
        set(_src "${OPENEMBER_SPDLOG_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_SPDLOG_CACHE_KEY}" "${OPENEMBER_SPDLOG_STAGE_DIR_NAME}"
            "${OPENEMBER_SPDLOG_URL}" "CMakeLists.txt" "")
    endif()

    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_SPDLOG_STAGE_DIR_NAME}-build")
endfunction()
