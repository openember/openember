aux_source_directory(. TEST_SRCS)

foreach(TEST_SRC ${TEST_SRCS})
    string(REGEX MATCH "^.*/([^/]+)[.]c$" TEST_NAME ${TEST_SRC})
    set(TEST_NAME ${CMAKE_MATCH_1})

    message(STATUS "${TEST_NAME} ${TEST_SRC}")

    AGLOO_ADD_TEST(TARGET   ${TEST_NAME} 
                   SRCS     ${TEST_SRC} 
                   LIBS     ${AGLOO_LIBRARIES} 
                   INCLUDES ${AGLOO_INCLUDE_DIRS} 
    )

endforeach(TEST_SRC)

file(GLOB CONF_FILES . *.conf)
file(COPY ${CONF_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})