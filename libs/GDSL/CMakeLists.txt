project(gdsl)

set( LIB_GDSL_SRCS 
    src/gdsl_2darray.c
    src/_gdsl_bintree.c
    src/_gdsl_bstree.c
    src/gdsl_bstree.c
    src/gdsl.c
    src/gdsl_hash.c
    src/gdsl_heap.c
    src/gdsl_interval_heap.c
    src/_gdsl_list.c
    src/gdsl_list.c
    src/_gdsl_node.c
    src/gdsl_perm.c
    src/gdsl_queue.c
    src/gdsl_rbtree.c
    src/gdsl_sort.c
    src/gdsl_stack.c
)

AGLOO_ADD_STATIC_LIBRARY (TARGET ${PROJECT_NAME} 
    SRCS     ${LIB_GDSL_SRCS}
    LIBS     ${AGLOO_EXTERNAL_LIBRARIES} 
    INCLUDES ${AGLOO_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src
    DESTINATION lib
)

set( GDSL_EXAMPLE_SRCS 
    examples/main_list.c
    examples/_strings.c
)

AGLOO_ADD_EXECUTABLE(TARGET gdsl_list_example
    SRCS     ${GDSL_EXAMPLE_SRCS}
    LIBS     ${AGLOO_LIBRARIES}
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/examples
    DESTINATION example
)
