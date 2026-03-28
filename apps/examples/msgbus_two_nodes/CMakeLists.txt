if(OPENEMBER_MSGBUS_USE_LCM)
    set(_msgbus_demo_addr "udpm://239.255.76.67:7667?ttl=1")
else()
    set(_msgbus_demo_addr "")
endif()

OPENEMBER_ADD_EXECUTABLE(TARGET msgbus_demo_publisher
    SRCS     publisher.cpp
    LIBS     ${OPENEMBER_LIBRARIES}
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
    DESTINATION bin
)
target_compile_definitions(msgbus_demo_publisher PRIVATE
    "OPENEMBER_MSGBUS_DEFAULT_ADDR=\"${_msgbus_demo_addr}\""
)

OPENEMBER_ADD_EXECUTABLE(TARGET msgbus_demo_subscriber
    SRCS     subscriber.cpp
    LIBS     ${OPENEMBER_LIBRARIES}
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
    DESTINATION bin
)
target_compile_definitions(msgbus_demo_subscriber PRIVATE
    "OPENEMBER_MSGBUS_DEFAULT_ADDR=\"${_msgbus_demo_addr}\""
)
