# yaml-cpp

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_prepare_yaml_cpp_source out_var)
    if(OPENEMBER_YAMLCPP_LOCAL_SOURCE)
        set(_src "${OPENEMBER_YAMLCPP_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_YAMLCPP_CACHE_KEY}" "${OPENEMBER_YAMLCPP_STAGE_DIR_NAME}"
            "${OPENEMBER_YAMLCPP_URL}" "CMakeLists.txt" "")
    endif()
    set(${out_var} "${_src}" PARENT_SCOPE)
endfunction()

function(openember_get_yaml_cpp)
    openember_prepare_yaml_cpp_source(_src)

    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)

    add_subdirectory("${_src}" "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_YAMLCPP_STAGE_DIR_NAME}-build")
endfunction()
