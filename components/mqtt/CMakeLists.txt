project(openember_mqtt C CXX)

set(MQTT_C_SRCS
    mqtt_client_wrapper.c
    mqtt_async_wrapper.c
    mosquitto_wrapper.c
)

OPENEMBER_ADD_STATIC_LIBRARY(
    TARGET ${PROJECT_NAME}
    SRCS     ${MQTT_C_SRCS} mqtt_client.cpp
    LIBS     ${OPENEMBER_EXTERNAL_LIBRARIES}
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION lib
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
