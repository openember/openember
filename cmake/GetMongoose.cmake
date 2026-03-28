# Mongoose（cesanta/mongoose）：单文件 mongoose.c 静态库

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_prepare_mongoose_source out_var)
    if(OPENEMBER_MONGOOSE_LOCAL_SOURCE)
        set(_src "${OPENEMBER_MONGOOSE_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_MONGOOSE_CACHE_KEY}" "${OPENEMBER_MONGOOSE_STAGE_DIR_NAME}"
            "${OPENEMBER_MONGOOSE_URL}" "mongoose.c" "")
    endif()
    set(${out_var} "${_src}" PARENT_SCOPE)
endfunction()

function(openember_get_mongoose)
    openember_prepare_mongoose_source(_src)

    if(NOT TARGET mongoose)
        add_library(mongoose STATIC "${_src}/mongoose.c")
        target_include_directories(mongoose PUBLIC "${_src}")
        set_target_properties(mongoose PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()

    set(MONGOOSE_INCLUDE_DIRS "${_src}" PARENT_SCOPE)
    set(MONGOOSE_LIBRARIES mongoose PARENT_SCOPE)
endfunction()
