project(network_sockets VERSION 0.1.0 LANGUAGES CXX)

set(_net_examples
    example_unix_echo.cpp
    example_tcp_echo.cpp
    example_udp_ping.cpp
)

foreach(_src ${_net_examples})
    get_filename_component(_name ${_src} NAME_WE)
    OPENEMBER_ADD_EXECUTABLE(TARGET ${_name}
        SRCS     ${_src}
        LIBS     ${OPENEMBER_LIBRARIES}
        INCLUDES ${OPENEMBER_INCLUDE_DIRS}
        DESTINATION bin
    )
    set_property(TARGET ${_name} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${_name} PROPERTY CXX_STANDARD_REQUIRED ON)
endforeach()

