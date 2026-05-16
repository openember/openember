# core/inc 公共头文件由根 CMakeLists.txt 的 configure_file / include 路径引用。
# C++ 运行时（Node / Topic / Service）在 OPENEMBER_ENABLE_CORE 时构建。

if(NOT DEFINED OPENEMBER_ENABLE_CORE OR NOT OPENEMBER_ENABLE_CORE)
    return()
endif()

if(NOT DEFINED OPENEMBER_COMPONENT_TRANSPORT OR NOT OPENEMBER_COMPONENT_TRANSPORT)
    message(FATAL_ERROR "OPENEMBER_ENABLE_CORE requires OPENEMBER_COMPONENT_TRANSPORT")
endif()

add_library(openember_core
    src/context.cpp
    src/init.cpp
    src/node.cpp
    src/topic/publisher_base.cpp
    src/topic/subscriber_base.cpp
    src/service/service_base.cpp
    src/service/client_base.cpp
)

target_include_directories(openember_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(openember_core
    PUBLIC
        cxx_std_17
)

target_link_libraries(openember_core
    PUBLIC
        openember::transport
)

add_library(openember::core ALIAS openember_core)
