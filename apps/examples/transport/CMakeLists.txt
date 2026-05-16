project(transport_examples VERSION 0.1.0 LANGUAGES CXX)

set(_transport_examples
    transport_listener.cpp
    transport_talker.cpp
)

foreach(_src ${_transport_examples})
    get_filename_component(_name ${_src} NAME_WE)
    OPENEMBER_ADD_EXECUTABLE(TARGET ${_name}
        SRCS     ${_src}
        LIBS     openember::transport
        INCLUDES ${OPENEMBER_INCLUDE_DIRS}
        DESTINATION bin
    )
    set_property(TARGET ${_name} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${_name} PROPERTY CXX_STANDARD_REQUIRED ON)
endforeach()
