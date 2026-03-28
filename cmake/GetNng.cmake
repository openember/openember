# nng

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_prepare_nng_source out_var)
    if(OPENEMBER_NNG_LOCAL_SOURCE)
        set(_src "${OPENEMBER_NNG_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_NNG_CACHE_KEY}" "${OPENEMBER_NNG_STAGE_DIR_NAME}"
            "${OPENEMBER_NNG_URL}" "CMakeLists.txt" "")
    endif()
    set(${out_var} "${_src}" PARENT_SCOPE)
endfunction()

function(openember_get_nng)
    openember_prepare_nng_source(_src)

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
    set(NNG_TESTS OFF CACHE BOOL "Build and run tests." FORCE)
    set(NNG_TOOLS OFF CACHE BOOL "Build extra tools." FORCE)
    set(NNG_ENABLE_COVERAGE OFF CACHE BOOL "Enable coverage reporting." FORCE)
    set(NNG_ENABLE_STATS OFF CACHE BOOL "Enable statistics." FORCE)
    set(NNG_ENABLE_HTTP OFF CACHE BOOL "Enable HTTP API." FORCE)
    set(NNG_TRANSPORT_WS OFF CACHE BOOL "Enable WebSocket transport." FORCE)
    set(NNG_TRANSPORT_WSS OFF CACHE BOOL "Enable WSS transport." FORCE)
    set(NNG_ENABLE_NNGCAT OFF CACHE BOOL "Enable building nngcat utility." FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_NNG_STAGE_DIR_NAME}-build")

    set(_inc "${_src}/include")
    set(_lib "")
    if(TARGET nng)
        set(_lib nng)
    endif()
    if(NOT _lib)
        set(_lib nng)
    endif()

    set(OPENEMBER_NNG_INCLUDE_DIRS ${_inc} PARENT_SCOPE)
    set(OPENEMBER_NNG_LIBRARIES ${_lib} PARENT_SCOPE)
endfunction()
