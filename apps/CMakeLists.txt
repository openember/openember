set(FOLDER_NAME "apps")

if(NOT DEFINED OPENEMBER_MODULE_LAUNCH_MANAGER OR OPENEMBER_MODULE_LAUNCH_MANAGER)
    add_subdirectory(system/launch_manager)
endif()

if(NOT DEFINED OPENEMBER_MODULE_ALOGD OR OPENEMBER_MODULE_ALOGD)
    add_subdirectory(system/log_service)
endif()

if(NOT DEFINED OPENEMBER_MODULE_DEVICE_MANAGER OR OPENEMBER_MODULE_DEVICE_MANAGER)
    add_subdirectory(system/device_manager)
endif()

if(NOT DEFINED OPENEMBER_MODULE_CONFIG_MANAGER OR OPENEMBER_MODULE_CONFIG_MANAGER)
    add_subdirectory(system/config_service)
endif()

if(NOT DEFINED OPENEMBER_MODULE_MONITOR_ALARM OR OPENEMBER_MODULE_MONITOR_ALARM)
    add_subdirectory(system/health_monitor)
endif()

if(NOT DEFINED OPENEMBER_MODULE_OTA OR OPENEMBER_MODULE_OTA)
    add_subdirectory(services/ota_update_service)
endif()

if(NOT DEFINED OPENEMBER_MODULE_ACQUISITION OR OPENEMBER_MODULE_ACQUISITION)
    add_subdirectory(references/sensor_data_reference)
endif()

if(NOT DEFINED OPENEMBER_MODULE_WEB_SERVER OR OPENEMBER_MODULE_WEB_SERVER)
    add_subdirectory(services/web_dashboard)
endif()

add_subdirectory(examples)
