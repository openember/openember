set(_openember_tests
    minunit_example.c
    test_log.c
)

if(OPENEMBER_LOG_BACKEND STREQUAL "ZLOG")
    list(APPEND _openember_tests test_zlog_conf.c)
endif()

foreach(_t ${_openember_tests})
    string(REPLACE ".c" "" TEST_NAME ${_t})
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
