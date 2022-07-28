project(MessageAPI)

set( MSG_WRAPPER_SRCS 
    msg_wrapper.c
    zmq_wrapper.c
    mqtt_wrapper.c
)

AGLOO_ADD_STATIC_LIBRARY (TARGET ${PROJECT_NAME} 
    SRCS     ${MSG_WRAPPER_SRCS}
    LIBS     ${AGLOO_EXTERNAL_LIBRARIES} 
    INCLUDES ${AGLOO_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION lib
)
