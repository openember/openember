# nlohmann/json（C++ header-only + CMake 目标 nlohmann_json::nlohmann_json）
include(FetchContent)

set(OPENEMBER_NLOHMANN_JSON_VERSION "3.11.3" CACHE STRING "nlohmann/json version tag")
set(OPENEMBER_NLOHMANN_JSON_URL
    "https://github.com/nlohmann/json/archive/refs/tags/v${OPENEMBER_NLOHMANN_JSON_VERSION}.tar.gz"
    CACHE STRING "nlohmann/json source archive URL")

function(openember_get_nlohmann_json)
    if(OPENEMBER_NLOHMANN_JSON_LOCAL_SOURCE)
        FetchContent_Declare(
            nlohmann_json
            SOURCE_DIR "${OPENEMBER_NLOHMANN_JSON_LOCAL_SOURCE}")
    else()
        FetchContent_Declare(
            nlohmann_json
            URL ${OPENEMBER_NLOHMANN_JSON_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()

    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(nlohmann_json)
endfunction()
