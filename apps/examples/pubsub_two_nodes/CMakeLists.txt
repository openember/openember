set(_ps_inc ${CMAKE_SOURCE_DIR}/components/pubsub)

OPENEMBER_ADD_EXECUTABLE(TARGET pubsub_publisher
    SRCS     publisher.cpp
    LIBS     ember_pubsub pthread ${OPENEMBER_LIBRARIES}
    INCLUDES ${_ps_inc} ${OPENEMBER_INCLUDE_DIRS}
    DESTINATION bin
)
target_compile_definitions(pubsub_publisher PRIVATE
    OPENEMBER_PUBSUB_DEFAULT_PUB_URL="${OPENEMBER_PUBSUB_DEMO_PUB_URL}"
)

OPENEMBER_ADD_EXECUTABLE(TARGET pubsub_subscriber
    SRCS     subscriber.cpp
    LIBS     ember_pubsub pthread ${OPENEMBER_LIBRARIES}
    INCLUDES ${_ps_inc} ${OPENEMBER_INCLUDE_DIRS}
    DESTINATION bin
)
target_compile_definitions(pubsub_subscriber PRIVATE
    OPENEMBER_PUBSUB_DEFAULT_SUB_URL="${OPENEMBER_PUBSUB_DEMO_SUB_URL}"
)
