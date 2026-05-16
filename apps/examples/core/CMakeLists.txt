project(core_examples VERSION 0.1.0 LANGUAGES CXX)

set(_core_examples
    topic_talker.cpp:openember_topic_talker
    topic_listener.cpp:openember_topic_listener
    service_server.cpp:openember_service_server
    service_client.cpp:openember_service_client
)

foreach(_pair ${_core_examples})
    string(REPLACE ":" ";" _parts "${_pair}")
    list(GET _parts 0 _src)
    list(GET _parts 1 _target)

    OPENEMBER_ADD_EXECUTABLE(TARGET ${_target}
        SRCS     ${_src}
        LIBS     openember::core
        INCLUDES ${OPENEMBER_INCLUDE_DIRS}
        DESTINATION bin
    )
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD_REQUIRED ON)
endforeach()
