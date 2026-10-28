project(agri_station_demo VERSION 0.1.0 LANGUAGES CXX)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_agri_mock_sensors
    SRCS     agri_mock_sensors.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_agri_station_app
    SRCS     agri_station_app.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

foreach(_target openember_agri_mock_sensors openember_agri_station_app)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD_REQUIRED ON)
endforeach()
