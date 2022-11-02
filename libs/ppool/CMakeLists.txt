project(ppool)

set( PPOOL_SRCS 
    ppool.c
    ppool_queue.c
    ppool_errno.c
)

AGLOO_ADD_STATIC_LIBRARY (TARGET ${PROJECT_NAME} 
    SRCS     ${PPOOL_SRCS}
    LIBS     ${AGLOO_EXTERNAL_LIBRARIES} 
    INCLUDES ${AGLOO_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION lib
)
