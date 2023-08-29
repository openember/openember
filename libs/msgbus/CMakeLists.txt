project(msgbus)

set( MSG_WRAPPER_SRCS 
    message.c
    zmq_wrapper.c
    mqtt_client_wrapper.c
    mqtt_async_wrapper.c
    mosquitto_wrapper.c
)

OPENEMBER_ADD_STATIC_LIBRARY (TARGET ${PROJECT_NAME} 
    SRCS     ${MSG_WRAPPER_SRCS}
    LIBS     ${OPENEMBER_EXTERNAL_LIBRARIES} 
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION lib
)
