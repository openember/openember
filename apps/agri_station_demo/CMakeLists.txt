project(agri_station_demo VERSION 0.1.0 LANGUAGES CXX)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_agri_mock_sensors
    SRCS     agri_mock_sensors.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

set(_agri_app_srcs agri_station_app.cpp)
set(_agri_app_libs openember::core)
set(_agri_app_defs)

option(OPENEMBER_AGRI_STATION_GUI "Build agri_station_app with Qt field console" ON)

if(OPENEMBER_AGRI_STATION_GUI)
    find_package(Qt6 QUIET COMPONENTS Widgets)
    if(Qt6_FOUND)
        list(APPEND _agri_app_srcs agri_station_view_qt.cpp)
        list(APPEND _agri_app_libs Qt6::Widgets)
        list(APPEND _agri_app_defs OPENEMBER_AGRI_STATION_GUI=1)
        set(CMAKE_AUTOMOC ON)
        message(STATUS "agri_station_demo: Qt6 Widgets field console enabled")
    else()
        find_package(Qt5 QUIET COMPONENTS Widgets)
        if(Qt5_FOUND)
            list(APPEND _agri_app_srcs agri_station_view_qt.cpp)
            list(APPEND _agri_app_libs Qt5::Widgets)
            list(APPEND _agri_app_defs OPENEMBER_AGRI_STATION_GUI=1)
            set(CMAKE_AUTOMOC ON)
            message(STATUS "agri_station_demo: Qt5 Widgets field console enabled")
        else()
            message(WARNING
                "OPENEMBER_AGRI_STATION_GUI=ON but Qt Widgets not found; "
                "building headless agri_station_app. Install qt6-base-dev "
                "(or qtbase5-dev) or set -DOPENEMBER_AGRI_STATION_GUI=OFF.")
            list(APPEND _agri_app_defs OPENEMBER_AGRI_STATION_GUI=0)
        endif()
    endif()
else()
    list(APPEND _agri_app_defs OPENEMBER_AGRI_STATION_GUI=0)
endif()

OPENEMBER_ADD_EXECUTABLE(TARGET openember_agri_station_app
    SRCS     ${_agri_app_srcs}
    LIBS     ${_agri_app_libs}
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

target_compile_definitions(openember_agri_station_app PRIVATE ${_agri_app_defs})

foreach(_target openember_agri_mock_sensors openember_agri_station_app)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD_REQUIRED ON)
endforeach()

if(OPENEMBER_AGRI_STATION_GUI AND (Qt6_FOUND OR Qt5_FOUND))
    set_target_properties(openember_agri_station_app PROPERTIES AUTOMOC ON)
endif()
