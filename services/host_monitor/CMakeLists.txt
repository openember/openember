project(host_monitor VERSION 0.2.0 LANGUAGES CXX)
get_target_property(_host_contract openember::msgs_cpp OPENEMBER_HOST_CONTRACT_VERSION)
if(NOT _host_contract OR _host_contract LESS 2)
    message(FATAL_ERROR "host_monitor requires openember-msgs host contract >= 2; update the messages source or select an updated local checkout")
endif()
unset(_host_contract)

OPENEMBER_ADD_EXECUTABLE(TARGET host_monitor
    SRCS     main.cpp host_sampler.cpp
    LIBS     ${OPENEMBER_LIBRARIES} openember_core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION bin
)

set_property(TARGET host_monitor PROPERTY CXX_STANDARD 17)
set_property(TARGET host_monitor PROPERTY CXX_STANDARD_REQUIRED ON)

if(OPENEMBER_ENABLE_TESTS)
    add_executable(host_sampler_test test/sampler_test.cpp host_sampler.cpp)
    target_include_directories(host_sampler_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(host_sampler_test PRIVATE cxx_std_17)
    add_test(NAME host_sampler COMMAND host_sampler_test)
endif()
