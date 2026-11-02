# Offline consumer fixture: verify CLI selection reaches a real CMake build.
cmake_minimum_required(VERSION 3.16)
project(workspace_cli_smoke C)
if(DEFINED ENV{OPENEMBER_TEST_FAIL_BUILD})
    add_custom_target(injected_build_failure ALL COMMAND ${CMAKE_COMMAND} -E false)
endif()
# Check propagation both during configure and in a real build subprocess.
if(NOT "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}" MATCHES "^[1-9][0-9]*$"
        OR NOT "$ENV{CARGO_BUILD_JOBS}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "Missing build concurrency environment")
endif()
add_custom_target(check_build_jobs ALL
    COMMAND ${CMAKE_COMMAND}
        "-DEXPECT_JOBS=$ENV{CMAKE_BUILD_PARALLEL_LEVEL}"
        "-DEXPECT_CARGO_JOBS=$ENV{CARGO_BUILD_JOBS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check-jobs.cmake")
include("${CMAKE_BINARY_DIR}/config.cmake")
if(NOT PRODUCT_MODEL STREQUAL OPENEMBER_PRODUCT_MODEL)
    message(FATAL_ERROR "Selected/configured model mismatch")
endif()
add_executable(launch_manager main.c)
set_target_properties(launch_manager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_target(openember_product ALL DEPENDS launch_manager)
# Simulate a framework service not linked by product apps: ember build must
# still honor its configuration and include it in the default build.
add_executable(framework_service main.c)
if(PRODUCT_BUILD_APP)
    add_executable(product_app main.c)
    add_dependencies(openember_product product_app)
endif()
