set(FOLDER_NAME "third_party")

if (BUILD_ZLOG)
    add_subdirectory(zlog)
endif()

# EasyLogger 已废弃（目前 OpenEmber 统一使用 zlog）

if (BUILD_CJSON)
    add_subdirectory(cJSON)
endif()

if (BUILD_MD5)
    add_subdirectory(md5)
endif()

if (BUILD_MQTT)
    add_subdirectory(paho.mqtt.c)
endif()

add_subdirectory(sqlite)