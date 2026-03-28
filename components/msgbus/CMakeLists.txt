project(msgbus)

set( MSG_WRAPPER_SRCS 
    message.c
    transport_backend_clib.cpp
    transport_backend_factory.cpp
    msgbus_node.cpp
    zmq_wrapper.c
    nng_wrapper.c
    lcm_wrapper.c
)

if(OPENEMBER_MSGBUS_USE_LCM AND OPENEMBER_MSGBUS_LCM_USE_CPP)
    list(APPEND MSG_WRAPPER_SRCS lcm_transport_backend.cpp)
endif()

OPENEMBER_ADD_STATIC_LIBRARY (TARGET ${PROJECT_NAME} 
    SRCS     ${MSG_WRAPPER_SRCS}
    LIBS     ${OPENEMBER_EXTERNAL_LIBRARIES} 
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION lib
)

if(OPENEMBER_MSGBUS_USE_LCM AND OPENEMBER_MSGBUS_LCM_USE_CPP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE OPENEMBER_MSGBUS_LCM_USE_CPP=1)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE OPENEMBER_MSGBUS_LCM_USE_CPP=0)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
