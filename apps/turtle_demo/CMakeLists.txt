project(turtle_demo VERSION 0.1.0 LANGUAGES CXX)

find_package(SDL2 REQUIRED)

set(_turtle_sdl_libs)
if(TARGET SDL2::SDL2)
    list(APPEND _turtle_sdl_libs SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    list(APPEND _turtle_sdl_libs SDL2::SDL2-static)
elseif(DEFINED SDL2_LIBRARIES)
    list(APPEND _turtle_sdl_libs ${SDL2_LIBRARIES})
else()
    message(FATAL_ERROR "SDL2 found but no usable CMake target/libraries")
endif()

OPENEMBER_ADD_EXECUTABLE(TARGET openember_turtle_sim
    SRCS     turtle_sim.cpp turtle_view_sdl.cpp
    LIBS     openember::core ${_turtle_sdl_libs}
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS}
    DESTINATION bin
)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_turtle_teleop
    SRCS     turtle_teleop.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

foreach(_target openember_turtle_sim openember_turtle_teleop)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD_REQUIRED ON)
endforeach()
