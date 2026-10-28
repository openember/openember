project(rover_demo VERSION 0.1.0 LANGUAGES CXX)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_rover_sim
    SRCS     rover_sim.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_rover_teleop
    SRCS     rover_teleop.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

foreach(_target openember_rover_sim openember_rover_teleop)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD_REQUIRED ON)
endforeach()
