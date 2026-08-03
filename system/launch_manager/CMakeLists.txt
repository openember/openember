project(launch_manager VERSION 1.0.0 LANGUAGES CXX)

set(LAUNCH_MANAGER_SRCS
    main.cpp
    launch_config.cpp
    process_manager.cpp
    fsm.cpp
    smm.cpp
)

if(NOT TARGET yaml-cpp::yaml-cpp)
    message(FATAL_ERROR "launch_manager requires yaml-cpp::yaml-cpp. Enable OPENEMBER_USE_YAMLCPP.")
endif()

OPENEMBER_ADD_EXECUTABLE(TARGET ${PROJECT_NAME}
    SRCS     ${LAUNCH_MANAGER_SRCS}
    LIBS     ${OPENEMBER_LIBRARIES} openember_core yaml-cpp::yaml-cpp
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
             ${CMAKE_SOURCE_DIR}/core/include
    DESTINATION bin
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
