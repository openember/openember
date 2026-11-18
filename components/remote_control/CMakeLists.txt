project(openember_remote_control VERSION 0.1.0 LANGUAGES CXX)
OPENEMBER_ADD_STATIC_LIBRARY(TARGET ${PROJECT_NAME}
    SRCS src/gamepad.cpp
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION lib)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(${PROJECT_NAME} PRIVATE src/f710_evdev.cpp)
endif()
add_library(openember::remote_control ALIAS ${PROJECT_NAME})
