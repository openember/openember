include_guard(GLOBAL)

# Build a product-owned protobuf library without adding its schemas to
# openember-msgs. The generated headers are propagated through the target, so
# consumers only need to link the requested namespaced alias.
function(openember_add_proto_library)
    cmake_parse_arguments(
        ARG
        ""
        "TARGET;ALIAS"
        "PROTOS;IMPORT_DIRS"
        ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "openember_add_proto_library requires TARGET")
    endif()
    if(NOT ARG_PROTOS)
        message(FATAL_ERROR "openember_add_proto_library requires PROTOS")
    endif()

    find_package(Protobuf REQUIRED)

    set(_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${_generated_dir}")

    add_library(${ARG_TARGET} STATIC)
    protobuf_generate(
        TARGET ${ARG_TARGET}
        LANGUAGE cpp
        APPEND_PATH
        PROTOS ${ARG_PROTOS}
        IMPORT_DIRS ${ARG_IMPORT_DIRS}
        PROTOC_OUT_DIR "${_generated_dir}")

    target_link_libraries(${ARG_TARGET} PUBLIC protobuf::libprotobuf)
    target_include_directories(${ARG_TARGET}
        PUBLIC
            "$<BUILD_INTERFACE:${_generated_dir}>"
            "$<INSTALL_INTERFACE:include>")
    target_compile_features(${ARG_TARGET} PUBLIC cxx_std_17)

    if(ARG_ALIAS)
        add_library(${ARG_ALIAS} ALIAS ${ARG_TARGET})
    endif()
endfunction()
