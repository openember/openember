# Standalone Asio（header-only）

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_prepare_asio_source out_var)
    if(OPENEMBER_ASIO_LOCAL_SOURCE)
        set(_src "${OPENEMBER_ASIO_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_ASIO_CACHE_KEY}" "${OPENEMBER_ASIO_STAGE_DIR_NAME}"
            "${OPENEMBER_ASIO_URL}" "asio/include/asio.hpp" "")
    endif()
    set(${out_var} "${_src}" PARENT_SCOPE)
endfunction()

function(openember_get_asio)
    openember_prepare_asio_source(_src)

    if(NOT TARGET openember::asio)
        add_library(openember_asio INTERFACE)
        add_library(openember::asio ALIAS openember_asio)
        target_include_directories(openember_asio INTERFACE
            $<BUILD_INTERFACE:${_src}/asio/include>)
        target_compile_definitions(openember_asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
        find_package(Threads REQUIRED)
        target_link_libraries(openember_asio INTERFACE Threads::Threads)
    endif()
endfunction()
