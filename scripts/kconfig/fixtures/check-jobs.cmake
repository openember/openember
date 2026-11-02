if(NOT "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}" STREQUAL "${EXPECT_JOBS}"
        OR NOT "$ENV{CARGO_BUILD_JOBS}" STREQUAL "${EXPECT_CARGO_JOBS}")
    message(FATAL_ERROR "Build subprocess did not inherit the configured jobs")
endif()
