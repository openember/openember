project(host_monitor VERSION 0.1.0 LANGUAGES CXX)

OPENEMBER_ADD_EXECUTABLE(TARGET host_monitor
    SRCS     main.cpp host_sampler.cpp
    LIBS     ${OPENEMBER_LIBRARIES} openember_core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

set_property(TARGET host_monitor PROPERTY CXX_STANDARD 17)
set_property(TARGET host_monitor PROPERTY CXX_STANDARD_REQUIRED ON)
