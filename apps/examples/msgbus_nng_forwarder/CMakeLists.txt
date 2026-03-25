set(_nng_inc ${OPENEMBER_NNG_INCLUDE_DIRS})

OPENEMBER_ADD_EXECUTABLE(TARGET msgbus_nng_forwarder
    SRCS     forwarder.cpp
    LIBS     ${OPENEMBER_NNG_LIBRARIES}
    INCLUDES ${_nng_inc}
    DESTINATION bin
)

