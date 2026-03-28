# ruckig — 在线 jerk 限制轨迹生成（MIT）
# https://github.com/pantor/ruckig
# 上游要求 CMake ≥3.15、库目标使用 C++20。

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_get_ruckig)
    if(OPENEMBER_RUCKIG_LOCAL_SOURCE)
        set(_src "${OPENEMBER_RUCKIG_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_RUCKIG_CACHE_KEY}" "${OPENEMBER_RUCKIG_STAGE_DIR_NAME}"
            "${OPENEMBER_RUCKIG_URL}" "CMakeLists.txt" "")
    endif()

    set(BUILD_EXAMPLES OFF CACHE BOOL "ruckig examples" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "ruckig tests" FORCE)
    set(BUILD_BENCHMARK OFF CACHE BOOL "ruckig benchmark" FORCE)
    set(BUILD_PYTHON_MODULE OFF CACHE BOOL "ruckig python" FORCE)
    set(BUILD_CLOUD_CLIENT OFF CACHE BOOL "ruckig cloud client" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "ruckig shared" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_RUCKIG_STAGE_DIR_NAME}-build")
endfunction()
