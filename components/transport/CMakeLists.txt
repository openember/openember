add_library(openember_transport
    src/key_builder.cpp
    src/session.cpp
    src/publisher.cpp
    src/subscriber.cpp
    src/service_server.cpp
    src/service_client.cpp
)

target_include_directories(openember_transport
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(openember_transport
    PUBLIC
        cxx_std_17
)

target_link_libraries(openember_transport
    PUBLIC
        zenohcxx::zenohc
)

add_library(openember::transport ALIAS openember_transport)