project(launch_manager VERSION 1.0.0 LANGUAGES CXX)

set(LAUNCH_MANAGER_SRCS
    main.cpp
    fsm.cpp
    smm.cpp
)

OPENEMBER_ADD_EXECUTABLE(TARGET ${PROJECT_NAME}
    SRCS     ${LAUNCH_MANAGER_SRCS}
    LIBS     ${OPENEMBER_LIBRARIES} openember_core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
             ${CMAKE_SOURCE_DIR}/core/include
    DESTINATION bin
)
