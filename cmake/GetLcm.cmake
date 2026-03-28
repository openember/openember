# lcm

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_get_lcm)
    if(OPENEMBER_LCM_LOCAL_SOURCE)
        set(_src "${OPENEMBER_LCM_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_LCM_CACHE_KEY}" "${OPENEMBER_LCM_STAGE_DIR_NAME}"
            "${OPENEMBER_LCM_URL}" "CMakeLists.txt" "")
    endif()

    set(LCM_ENABLE_EXAMPLES OFF CACHE BOOL "Build test and example programs" FORCE)
    set(LCM_ENABLE_TESTS OFF CACHE BOOL "Build unit tests" FORCE)
    set(LCM_ENABLE_PYTHON OFF CACHE BOOL "Build Python bindings and utilities" FORCE)
    set(LCM_ENABLE_JAVA OFF CACHE BOOL "Build Java bindings and utilities" FORCE)
    set(LCM_ENABLE_LUA OFF CACHE BOOL "Build Lua bindings and utilities" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_LCM_STAGE_DIR_NAME}-build")

    set(_inc "${_src}")
    set(_lib "")
    if(TARGET lcm-static)
        set(_lib lcm-static)
    elseif(TARGET lcm)
        set(_lib lcm)
    endif()
    if(NOT _lib)
        set(_lib lcm-static)
    endif()

    set(OPENEMBER_LCM_INCLUDE_DIRS ${_inc} PARENT_SCOPE)
    set(OPENEMBER_LCM_LIBRARIES ${_lib} PARENT_SCOPE)
endfunction()
