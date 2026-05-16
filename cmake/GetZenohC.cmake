# zenoh-c (https://github.com/eclipse-zenoh/zenoh-c) — Rust/Cargo 构建 C 库

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_prepare_zenoh_c_source out_var)
    if(OPENEMBER_ZENOHC_LOCAL_SOURCE)
        set(_src "${OPENEMBER_ZENOHC_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_ZENOHC_CACHE_KEY}" "${OPENEMBER_ZENOHC_STAGE_DIR_NAME}"
            "${OPENEMBER_ZENOHC_URL}" "CMakeLists.txt" "")
    endif()
    set(${out_var} "${_src}" PARENT_SCOPE)
endfunction()

function(openember_get_zenoh_c)
    if(TARGET zenohc::lib)
        return()
    endif()

    openember_prepare_zenoh_c_source(_src)

    find_program(_cargo NAMES cargo)
    if(NOT _cargo)
        message(FATAL_ERROR
            "zenoh-c requires Rust/Cargo on PATH (https://rustup.rs). "
            "Install rustup or set OPENEMBER_THIRD_PARTY_MODE=SYSTEM with zenohc installed.")
    endif()

    set(ZENOHC_BUILD_IN_SOURCE_TREE OFF CACHE BOOL "Do not build inside zenoh-c source tree" FORCE)
    set(ZENOHC_COPY_SOURCE_CARGO_LOCK ON CACHE BOOL "Copy Cargo.lock for reproducible builds" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Link zenoh-c statically" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_ZENOHC_STAGE_DIR_NAME}-build")

    if(NOT TARGET zenohc::lib)
        message(FATAL_ERROR "zenoh-c: expected CMake target zenohc::lib after add_subdirectory")
    endif()

    set(OPENEMBER_ZENOHC_LIBRARIES zenohc::lib PARENT_SCOPE)
endfunction()
