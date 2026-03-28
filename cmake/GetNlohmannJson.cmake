# nlohmann/json（header-only + CMake 目标 nlohmann_json::nlohmann_json）

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_get_nlohmann_json)
    if(OPENEMBER_NLOHMANN_JSON_LOCAL_SOURCE)
        set(_src "${OPENEMBER_NLOHMANN_JSON_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_NLOHMANN_JSON_CACHE_KEY}" "${OPENEMBER_NLOHMANN_JSON_STAGE_DIR_NAME}"
            "${OPENEMBER_NLOHMANN_JSON_URL}" "CMakeLists.txt" "")
    endif()

    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_NLOHMANN_JSON_STAGE_DIR_NAME}-build")
endfunction()
