# zenoh-cpp (https://github.com/eclipse-zenoh/zenoh-cpp) — header-only C++17，依赖 zenoh-c

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_prepare_zenoh_cpp_source out_var)
    if(OPENEMBER_ZENOHCXX_LOCAL_SOURCE)
        set(_src "${OPENEMBER_ZENOHCXX_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_ZENOHCXX_CACHE_KEY}" "${OPENEMBER_ZENOHCXX_STAGE_DIR_NAME}"
            "${OPENEMBER_ZENOHCXX_URL}" "include/zenoh.hxx" "")
    endif()
    set(${out_var} "${_src}" PARENT_SCOPE)
endfunction()

function(openember_get_zenoh_cpp)
    if(TARGET zenohcxx::zenohc)
        return()
    endif()

    if(NOT TARGET zenohc::lib)
        message(FATAL_ERROR
            "zenoh-cpp requires zenoh-c: call openember_get_zenoh_c() or openember_transport_resolve_zenoh_c() first.")
    endif()

    openember_prepare_zenoh_cpp_source(_src)

    set(ZENOHCXX_ZENOHC ON CACHE BOOL "Build zenoh-cpp for zenoh-c backend" FORCE)
    set(ZENOHCXX_ZENOHPICO OFF CACHE BOOL "Disable zenoh-pico backend" FORCE)
    set(ZENOHCXX_ENABLE_TESTS OFF CACHE BOOL "Disable zenoh-cpp tests" FORCE)
    set(ZENOHCXX_ENABLE_EXAMPLES OFF CACHE BOOL "Disable zenoh-cpp examples" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_ZENOHCXX_STAGE_DIR_NAME}-build")

    if(NOT TARGET zenohcxx::zenohc)
        message(FATAL_ERROR "zenoh-cpp: expected CMake target zenohcxx::zenohc after add_subdirectory")
    endif()

    set(OPENEMBER_ZENOHCXX_LIBRARIES zenohcxx::zenohc PARENT_SCOPE)
endfunction()
