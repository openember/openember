set(_openember_tests
    minunit_example.c
    test_log.c
    test_msgbus_transport.cpp
)

if(OPENEMBER_ENABLE_OSAL)
    list(APPEND _openember_tests test_osal.c)
endif()

if(OPENEMBER_ENABLE_HAL)
    list(APPEND _openember_tests test_hal.c)
endif()

if(OPENEMBER_LOG_BACKEND STREQUAL "ZLOG")
    list(APPEND _openember_tests test_zlog_conf.c)
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
endforeach()

file(GLOB CONF_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.conf)
if(CONF_FILES)
    file(COPY ${CONF_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()
