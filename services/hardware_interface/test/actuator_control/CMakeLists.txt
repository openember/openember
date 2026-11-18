cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(openember_hi_actuator_control_tests LANGUAGES CXX)
    enable_testing()
    get_filename_component(_core "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
    set(OPENEMBER_MSGS_SOURCE_DIR "${_core}/../openember-msgs" CACHE PATH "Local actuator/v2 message checkout")
    set(OPENEMBER_MSGS_BUILD_CPP ON CACHE BOOL "" FORCE)
    set(OPENEMBER_MSGS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${OPENEMBER_MSGS_SOURCE_DIR}" msgs)
    add_library(hi_actuator_control_under_test STATIC
        "${_core}/components/actuator/src/control_guard.cpp"
        "${_core}/services/hardware_interface/adapters/actuator_control_adapter.cpp"
        "${_core}/services/hardware_interface/src/joint_control_session.cpp"
        "${_core}/services/hardware_interface/src/joint_control_loop.cpp")
    target_compile_features(hi_actuator_control_under_test PUBLIC cxx_std_17)
    target_include_directories(hi_actuator_control_under_test PUBLIC
        "${_core}/components/actuator/include" "${_core}/components/hardware/include"
        "${_core}/services/hardware_interface/include")
    target_link_libraries(hi_actuator_control_under_test PUBLIC openember::msgs_cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(hi_actuator_control_under_test PUBLIC Threads::Threads)
    set(_hi_control_target hi_actuator_control_under_test)
else()
    set(_hi_control_target openember::hardware_interface)
endif()

add_executable(test_actuator_control_adapter actuator_control_adapter_test.cpp)
target_compile_features(test_actuator_control_adapter PRIVATE cxx_std_17)
target_link_libraries(test_actuator_control_adapter PRIVATE ${_hi_control_target})
foreach(_case command request capabilities result feedback session malformed lifecycle)
    add_test(NAME actuator_v2_${_case} COMMAND test_actuator_control_adapter ${_case})
    set_tests_properties(actuator_v2_${_case} PROPERTIES LABELS "regression;unit" TIMEOUT 10)
endforeach()

add_executable(test_joint_control_loop joint_control_loop_test.cpp)
target_compile_features(test_joint_control_loop PRIVATE cxx_std_17)
target_link_libraries(test_joint_control_loop PRIVATE ${_hi_control_target})
foreach(_case lifecycle priority overflow expiry feedback exception concurrent)
    add_test(NAME actuator_loop_${_case} COMMAND test_joint_control_loop ${_case})
    set_tests_properties(actuator_loop_${_case} PROPERTIES LABELS "regression;integration" TIMEOUT 15)
endforeach()
