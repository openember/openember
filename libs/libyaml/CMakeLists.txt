project(yaml)

set( LIBYAML_SRCS 
    src/api.c
    src/dumper.c
    src/emitter.c
    src/loader.c
    src/parser.c
    src/reader.c
    src/scanner.c
    src/writer.c
)

set( LIBYAML_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

OPENEMBER_ADD_STATIC_LIBRARY (TARGET ${PROJECT_NAME} 
    SRCS     ${LIBYAML_SRCS}
    LIBS     ${OPENEMBER_EXTERNAL_LIBRARIES} 
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${LIBYAML_INCLUDE_DIRS}
    DESTINATION lib
)
