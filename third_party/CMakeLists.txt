set(FOLDER_NAME "third_party")

if (BUILD_ZLOG)
    add_subdirectory(zlog)
endif()

if (BUILD_EASYLOGGER)
    add_subdirectory(easylogger)
endif()

if (BUILD_CJSON)
    add_subdirectory(cJSON)
endif()

if (BUILD_MD5)
    add_subdirectory(md5)
endif()

if (BUILD_MQTT)
    add_subdirectory(paho.mqtt.c)
endif()
