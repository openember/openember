set(_openember_tests
    minunit_example.c
    test_algorithm.cpp
    test_log.cpp
)

if(OPENEMBER_ENABLE_OSAL)
    list(APPEND _openember_tests test_osal.cpp)
endif()

if(OPENEMBER_COMPONENT_ACTUATOR)
    list(APPEND _openember_tests test_actuator.cpp)
endif()

foreach(_t ${_openember_tests})
    set(TEST_NAME "${_t}")
    string(REPLACE ".cpp" "" TEST_NAME "${TEST_NAME}")
    string(REPLACE ".c" "" TEST_NAME "${TEST_NAME}")
    message(STATUS "${TEST_NAME} ${_t}")
    OPENEMBER_ADD_TEST(TARGET   ${TEST_NAME}
                   SRCS     ${_t}
                   LIBS     ${OPENEMBER_LIBRARIES}
                   INCLUDES ${OPENEMBER_INCLUDE_DIRS}
    )
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "regression;unit" TIMEOUT 30)
endforeach()

if(OPENEMBER_REGRESSION_BASELINE)
    foreach(_required test_algorithm test_osal test_actuator launch_manager
            openember_hardware_interface openember_hardware_mock_listener
            openember_joint_command_sender)
        if(NOT TARGET ${_required})
            message(FATAL_ERROR "Regression baseline requires target ${_required}")
        endif()
    endforeach()
    foreach(_endpoint IMU TEMPERATURE GNSS JOINT_CONTROLLER)
        if(NOT OPENEMBER_HARDWARE_ENDPOINT_${_endpoint})
            message(FATAL_ERROR "Regression baseline requires ${_endpoint} endpoint")
        endif()
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    if(TARGET launch_manager OR TARGET openember_hardware_interface)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
    endif()
    if(TARGET launch_manager)
        add_test(NAME regression_supervision
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/runtime_regression.py
                supervision --launch-manager $<TARGET_FILE:launch_manager>)
        set_tests_properties(regression_supervision PROPERTIES
            LABELS "regression;integration" TIMEOUT 45)
    endif()
    if(TARGET openember_hardware_interface AND TARGET openember_hardware_mock_listener
            AND TARGET openember_joint_command_sender
            AND OPENEMBER_HARDWARE_ENDPOINT_IMU AND OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE
            AND OPENEMBER_HARDWARE_ENDPOINT_GNSS AND OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER)
        add_test(NAME regression_hardware
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/runtime_regression.py
                hardware --hardware $<TARGET_FILE:openember_hardware_interface>
                --listener $<TARGET_FILE:openember_hardware_mock_listener>
                --sender $<TARGET_FILE:openember_joint_command_sender>
                --source ${CMAKE_SOURCE_DIR})
        set_tests_properties(regression_hardware PROPERTIES
            LABELS "regression;integration" TIMEOUT 60 RESOURCE_LOCK openember_local_router)
    endif()
elseif(OPENEMBER_REGRESSION_BASELINE)
    message(FATAL_ERROR "Regression baseline requires a native Linux build")
endif()

file(GLOB CONF_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.conf)
if(CONF_FILES)
    file(COPY ${CONF_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()
