cmake_minimum_required(VERSION 3.16)

# This dependency-free test suite can run alone or through OpenEmber's test/.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(openember_actuator_control_tests LANGUAGES CXX)
    enable_testing()
    add_library(actuator_control_under_test STATIC ../src/control_guard.cpp)
    target_compile_features(actuator_control_under_test PUBLIC cxx_std_17)
    target_include_directories(actuator_control_under_test PUBLIC
        ../include ../../hardware/include)
    set(_actuator_control_target actuator_control_under_test)
else()
    set(_actuator_control_target openember_actuator)
endif()

add_executable(test_actuator_control_guard control_guard_test.cpp)
target_compile_features(test_actuator_control_guard PRIVATE cxx_std_17)
target_link_libraries(test_actuator_control_guard PRIVATE ${_actuator_control_target})
foreach(_case capabilities targets context lease idempotency watchdog execution safety pause_reset resources feedback clock arm_preflight)
    add_test(NAME actuator_control_${_case} COMMAND test_actuator_control_guard ${_case})
    set_tests_properties(actuator_control_${_case} PROPERTIES LABELS "regression;unit" TIMEOUT 10)
endforeach()
