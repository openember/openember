# libzmq

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_get_libzmq)
    if(OPENEMBER_LIBZMQ_LOCAL_SOURCE)
        set(_src "${OPENEMBER_LIBZMQ_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_LIBZMQ_CACHE_KEY}" "${OPENEMBER_LIBZMQ_STAGE_DIR_NAME}"
            "${OPENEMBER_LIBZMQ_URL}" "CMakeLists.txt" "")
    endif()

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Build tests (CMake conventional)" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "Build tests (upstream convention)" FORCE)
    set(ZMQ_BUILD_TESTS OFF CACHE BOOL "Build libzmq tests" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_LIBZMQ_STAGE_DIR_NAME}-build")

    set(_inc "${_src}/include")
    set(_lib "")
    if(TARGET libzmq-static)
        set(_lib libzmq-static)
    elseif(TARGET libzmq)
        set(_lib libzmq)
    endif()
    if(NOT _lib)
        set(_lib libzmq)
    endif()

    set(OPENEMBER_LIBZMQ_INCLUDE_DIRS ${_inc} PARENT_SCOPE)
    set(OPENEMBER_LIBZMQ_LIBRARIES ${_lib} PARENT_SCOPE)
endfunction()
