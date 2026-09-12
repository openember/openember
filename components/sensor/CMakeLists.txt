project(openember_sensor VERSION 0.1.0 LANGUAGES CXX)

set(OPENEMBER_SENSOR_SRCS
    src/mock_gnss.cpp
    src/mock_imu.cpp
    src/mock_temperature.cpp
)

OPENEMBER_ADD_STATIC_LIBRARY(TARGET ${PROJECT_NAME}
    SRCS     ${OPENEMBER_SENSOR_SRCS}
    LIBS     ${OPENEMBER_EXTERNAL_LIBRARIES} openember_hardware
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
             ${CMAKE_SOURCE_DIR}/components/hardware/include
             ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION lib
)

add_library(openember::sensor ALIAS ${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
