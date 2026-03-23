set(FOLDER_NAME "components")

add_subdirectory(Log)
add_subdirectory(HAL)
add_subdirectory(GDSL)
add_subdirectory(msgbus)
add_subdirectory(Common)
add_subdirectory(libcsv)
add_subdirectory(libyaml)
add_subdirectory(ppool)

if(BUILD_PUBSUB_ZMQ OR BUILD_PUBSUB_NNG OR BUILD_PUBSUB_LCM)
    add_subdirectory(pubsub)
endif()