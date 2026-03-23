# yaml-cpp（C++ YAML 解析）
include(FetchContent)

set(OPENEMBER_YAMLCPP_VERSION "0.8.0" CACHE STRING "yaml-cpp version")
# 与 GitHub 源码包目录名一致（解压为 yaml-cpp-<version>）
set(OPENEMBER_YAMLCPP_URL
    "https://github.com/jbeder/yaml-cpp/archive/${OPENEMBER_YAMLCPP_VERSION}.tar.gz"
    CACHE STRING "yaml-cpp archive URL")

function(openember_get_yaml_cpp)
    if(OPENEMBER_YAMLCPP_LOCAL_SOURCE)
        FetchContent_Declare(
            yaml-cpp
            SOURCE_DIR "${OPENEMBER_YAMLCPP_LOCAL_SOURCE}")
    else()
        FetchContent_Declare(
            yaml-cpp
            URL ${OPENEMBER_YAMLCPP_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()

    FetchContent_GetProperties(yaml-cpp)
    if(NOT yaml-cpp_POPULATED)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(yaml-cpp)
    endif()
endfunction()
