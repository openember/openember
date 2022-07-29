
macro(AGLOO_SET_VERSION PROJECT_NAME VERSION_MAJOR VERSION_MINOR VERSION_PATCH)
# Set CPACK version and project name
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})
set(AGLOO_VERSION ${VERSION} )
endmacro(AGLOO_SET_VERSION)

macro(AGLOO_ADD_SOURCE_DIR SRC_DIR SRC_VAR)
# Create the list of expressions to be used in the search.
set(GLOB_EXPRESSIONS "")
foreach(ARG ${ARGN})
    list(APPEND GLOB_EXPRESSIONS ${SRC_DIR}/${ARG})
endforeach()
# Perform the search for the source files.
file(GLOB ${SRC_VAR} RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${GLOB_EXPRESSIONS})
# Create the source group.
string(REPLACE "/" "\\" GROUP_NAME ${SRC_DIR})
source_group(${GROUP_NAME} FILES ${${SRC_VAR}})
# Clean-up.
unset(GLOB_EXPRESSIONS)
unset(ARG)
unset(GROUP_NAME)
endmacro(AGLOO_ADD_SOURCE_DIR)

macro(AGLOO_ADD_SOURCES)
set(SOURCE_FILES "")
foreach(SOURCE_FILE ${ARGN})
    if(SOURCE_FILE MATCHES "^/.*")
        list(APPEND SOURCE_FILES ${SOURCE_FILE})
    else()
        list(APPEND SOURCE_FILES
             "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}")
    endif()
endforeach()
set(PROJECT_SOURCES ${PROJECT_SOURCES} ${SOURCE_FILES} PARENT_SCOPE)
endmacro(AGLOO_ADD_SOURCES)

# https://stackoverflow.com/questions/23327687/how-to-write-a-cmake-function-with-more-than-one-parameter-groups
macro(AGLOO_ADD_EXECUTABLE )
cmake_parse_arguments(
    PARSED_ARGS # prefix of output variables
    "" # list of names of the boolean arguments (only defined ones will be true)
    "TARGET;DESTINATION" # list of names of mono-valued arguments
    "SRCS;LIBS;INCLUDES" # list of names of multi-valued arguments (output variables are lists)
    ${ARGN} # arguments of the function to parse, here we take the all original ones
)
# note: if it remains unparsed arguments, here, they can be found in variable PARSED_ARGS_UNPARSED_ARGUMENTS
if(NOT PARSED_ARGS_TARGET)
    message(FATAL_ERROR "You must provide a target")
endif()
set ( TARGET_NAME ${PARSED_ARGS_TARGET})
aux_source_directory("${CMAKE_HOME_DIRECTORY}/src" AGLOO_SRCS)
add_executable(${TARGET_NAME} ${PARSED_ARGS_SRCS} ${AGLOO_SRCS})
if(PARSED_ARGS_DESTINATION)
set_target_properties(${TARGET_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PARSED_ARGS_DESTINATION}" 
)
endif()
if(PARSED_ARGS_LIBS)
    target_link_libraries(${TARGET_NAME} ${PARSED_ARGS_LIBS})
endif()
if (PARSED_ARGS_INCLUDES)
    target_include_directories(${TARGET_NAME} PUBLIC ${PARSED_ARGS_INCLUDES})
endif()
install(TARGETS ${TARGET_NAME}
    EXPORT ${TARGET_NAME}-target
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
endmacro(AGLOO_ADD_EXECUTABLE)

macro(AGLOO_ADD_LIBRARY )
cmake_parse_arguments(
    PARSED_ARGS # prefix of output variables
    "" # list of names of the boolean arguments (only defined ones will be true)
    "TARGET;DESTINATION" # list of names of mono-valued arguments
    "SRCS;LIBS;INCLUDES" # list of names of multi-valued arguments (output variables are lists)
    ${ARGN} # arguments of the function to parse, here we take the all original ones
)
# note: if it remains unparsed arguments, here, they can be found in variable PARSED_ARGS_UNPARSED_ARGUMENTS
if(NOT PARSED_ARGS_TARGET)
    message(FATAL_ERROR "You must provide a target")
endif()
set ( TARGET_NAME ${PARSED_ARGS_TARGET})
add_library(${TARGET_NAME} SHARED ${PARSED_ARGS_SRCS})
if(PARSED_ARGS_DESTINATION)
set_target_properties(${TARGET_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PARSED_ARGS_DESTINATION}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PARSED_ARGS_DESTINATION}"
)
endif()
if(PARSED_ARGS_LIBS)
    target_link_libraries(${TARGET_NAME} ${PARSED_ARGS_LIBS})
endif()
if (PARSED_ARGS_INCLUDES)
    target_include_directories(${TARGET_NAME} PUBLIC ${PARSED_ARGS_INCLUDES})
endif()
install(TARGETS ${TARGET_NAME} EXPORT ${TARGET_NAME}-target
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
endmacro(AGLOO_ADD_LIBRARY)

macro(AGLOO_ADD_STATIC_LIBRARY )
cmake_parse_arguments(
    PARSED_ARGS # prefix of output variables
    "" # list of names of the boolean arguments (only defined ones will be true)
    "TARGET;DESTINATION;" # list of names of mono-valued arguments
    "SRCS;LIBS;INCLUDES" # list of names of multi-valued arguments (output variables are lists)
    ${ARGN} # arguments of the function to parse, here we take the all original ones
)
# note: if it remains unparsed arguments, here, they can be found in variable PARSED_ARGS_UNPARSED_ARGUMENTS
if(NOT PARSED_ARGS_TARGET)
    message(FATAL_ERROR "You must provide a target")
endif()
set ( TARGET_NAME ${PARSED_ARGS_TARGET})
add_library(${TARGET_NAME} STATIC ${PARSED_ARGS_SRCS})
if(PARSED_ARGS_DESTINATION)
set_target_properties(${TARGET_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PARSED_ARGS_DESTINATION}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PARSED_ARGS_DESTINATION}"
)
endif()
if(PARSED_ARGS_LIBS)
    target_link_libraries(${TARGET_NAME} ${PARSED_ARGS_LIBS})
endif()
if (PARSED_ARGS_INCLUDES)
    target_include_directories(${TARGET_NAME} PUBLIC ${PARSED_ARGS_INCLUDES})
endif()
install(TARGETS ${TARGET_NAME} EXPORT ${TARGET_NAME}-target
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
endmacro(AGLOO_ADD_STATIC_LIBRARY)

macro(AGLOO_ADD_TEST)
if(TESTS_ENABLED)
    cmake_parse_arguments(
      PARSED_ARGS # prefix of output variables
      "" # list of names of the boolean arguments (only defined ones will be true)
      "TARGET" # list of names of mono-valued arguments
      "SRCS;LIBS;INCLUDES" # list of names of multi-valued arguments (output variables are lists)
      ${ARGN} # arguments of the function to parse, here we take the all original ones
    )

    set ( TARGET_NAME ${PARSED_ARGS_TARGET})
    aux_source_directory("${CMAKE_HOME_DIRECTORY}/src" AGLOO_SRCS)
    add_executable(${TARGET_NAME} ${PARSED_ARGS_SRCS} ${AGLOO_SRCS})
    if(PARSED_ARGS_LIBS)
      target_link_libraries(${TARGET_NAME} ${PARSED_ARGS_LIBS} ${GTEST_LIBRARIES})
    endif()
    if (PARSED_ARGS_INCLUDES)
      target_include_directories(${TARGET_NAME} PUBLIC ${PARSED_ARGS_INCLUDES})
    endif()

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_target_properties(${TARGET_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test" 
    )
    if(IS_MSVC)
        install(TARGETS ${TARGET_NAME} DESTINATION bin/)
    endif()
endif()
endmacro(AGLOO_ADD_TEST)
