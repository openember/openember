project(MessageAPI)

set( MSG_WRAPPER_SRCS 
    message.c
    zmq_wrapper.c
    mqtt_client_wrapper.c
    mqtt_async_wrapper.c
    mosquitto_wrapper.c
)

AGLOO_ADD_STATIC_LIBRARY (TARGET ${PROJECT_NAME} 
    SRCS     ${MSG_WRAPPER_SRCS}
    LIBS     ${AGLOO_EXTERNAL_LIBRARIES} 
    INCLUDES ${AGLOO_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION lib
)
