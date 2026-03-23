# spdlog（C++ 日志，header-only + 可选编译）
include(FetchContent)

set(OPENEMBER_SPDLOG_VERSION "1.14.1" CACHE STRING "spdlog version")
set(OPENEMBER_SPDLOG_URL
    "https://github.com/gabime/spdlog/archive/refs/tags/v${OPENEMBER_SPDLOG_VERSION}.tar.gz"
    CACHE STRING "spdlog archive URL")

function(openember_get_spdlog)
    if(OPENEMBER_SPDLOG_LOCAL_SOURCE)
        FetchContent_Declare(
            spdlog
            SOURCE_DIR "${OPENEMBER_SPDLOG_LOCAL_SOURCE}")
    else()
        FetchContent_Declare(
            spdlog
            URL ${OPENEMBER_SPDLOG_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()

    FetchContent_GetProperties(spdlog)
    if(NOT spdlog_POPULATED)
        set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
        set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
        set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(spdlog)
    endif()
endfunction()
