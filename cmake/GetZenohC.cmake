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

# 读取 zenoh-c/rust-toolchain.toml 中的 channel，并检测 rustup 是否可用。
function(openember_zenoh_select_cargo_channel zenoh_src)
    if(OPENEMBER_ZENOHC_CARGO_CHANNEL AND NOT OPENEMBER_ZENOHC_CARGO_CHANNEL STREQUAL "auto")
        set(ZENOHC_CARGO_CHANNEL "${OPENEMBER_ZENOHC_CARGO_CHANNEL}" CACHE STRING
            "Cargo channel parameter for zenoh-c (zenoh upstream)" FORCE)
        message(STATUS "zenoh-c: using OPENEMBER_ZENOHC_CARGO_CHANNEL=${OPENEMBER_ZENOHC_CARGO_CHANNEL}")
        return()
    endif()

    set(_required "1.93.0")
    if(EXISTS "${zenoh_src}/rust-toolchain.toml")
        file(READ "${zenoh_src}/rust-toolchain.toml" _rtc)
        string(REGEX MATCH "channel[ \t]*=[ \t]*\"([^\"]+)\"" _m "${_rtc}")
        if(CMAKE_MATCH_1)
            set(_required "${CMAKE_MATCH_1}")
        endif()
    endif()

    find_program(_rustup NAMES rustup)
    if(NOT _rustup)
        set(ZENOHC_CARGO_CHANNEL "+stable" CACHE STRING
            "Cargo channel parameter for zenoh-c (zenoh upstream)" FORCE)
        message(WARNING
            "zenoh-c: rustup not found; using cargo +stable. Install rustup for pinned toolchain ${_required}.")
        return()
    endif()

    execute_process(
        COMMAND ${_rustup} run ${_required} rustc -vV
        RESULT_VARIABLE _rv
        ERROR_VARIABLE _err
        OUTPUT_QUIET
    )
    if(_rv EQUAL 0)
        set(ZENOHC_CARGO_CHANNEL "" CACHE STRING
            "Cargo channel parameter for zenoh-c (zenoh upstream)" FORCE)
        message(STATUS "zenoh-c: rust-toolchain ${_required} OK (rustup run ${_required})")
        return()
    endif()

    execute_process(
        COMMAND ${_rustup} run stable rustc -vV
        RESULT_VARIABLE _stable_rv
        ERROR_VARIABLE _stable_err
        OUTPUT_QUIET
    )
    if(NOT _stable_rv EQUAL 0)
        message(FATAL_ERROR
            "zenoh-c: Rust toolchain ${_required} is broken (${_err}), and stable is unavailable (${_stable_err}).\n"
            "Repair with: rustup toolchain uninstall ${_required}\n"
            "             rustup toolchain install ${_required}\n"
            "Or set -DOPENEMBER_ZENOHC_CARGO_CHANNEL=+stable after fixing rustup.")
    endif()

    set(ZENOHC_CARGO_CHANNEL "+stable" CACHE STRING
        "Cargo channel parameter for zenoh-c (zenoh upstream)" FORCE)
    message(WARNING
        "zenoh-c: rustup toolchain '${_required}' failed (${_err}).\n"
        "  Falling back to cargo +stable for this build.\n"
        "  To use the pinned toolchain: rustup toolchain uninstall ${_required}\n"
        "                               rustup toolchain install ${_required}\n"
        "  Or force: -DOPENEMBER_ZENOHC_CARGO_CHANNEL=+stable")
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

    openember_zenoh_select_cargo_channel("${_src}")

    set(ZENOHC_BUILD_IN_SOURCE_TREE OFF CACHE BOOL "Do not build inside zenoh-c source tree" FORCE)
    set(ZENOHC_COPY_SOURCE_CARGO_LOCK ON CACHE BOOL "Copy Cargo.lock for reproducible builds" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Link zenoh-c statically" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_ZENOHC_STAGE_DIR_NAME}-build")

    if(NOT TARGET zenohc::lib)
        message(FATAL_ERROR "zenoh-c: expected CMake target zenohc::lib after add_subdirectory")
    endif()

    set(OPENEMBER_ZENOHC_LIBRARIES zenohc::lib PARENT_SCOPE)
endfunction()
