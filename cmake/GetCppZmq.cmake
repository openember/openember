# cppzmq（头文件为主）

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_prepare_cppzmq_source out_var)
    if(OPENEMBER_CPPZMQ_LOCAL_SOURCE)
        set(_src "${OPENEMBER_CPPZMQ_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_CPPZMQ_CACHE_KEY}" "${OPENEMBER_CPPZMQ_STAGE_DIR_NAME}"
            "${OPENEMBER_CPPZMQ_URL}" "zmq.hpp" "")
    endif()
    set(${out_var} "${_src}" PARENT_SCOPE)
endfunction()

function(openember_get_cppzmq)
    openember_prepare_cppzmq_source(_src)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_CPPZMQ_STAGE_DIR_NAME}-build")

    set(OPENEMBER_CPPZMQ_INCLUDE_DIRS ${_src} PARENT_SCOPE)
endfunction()
