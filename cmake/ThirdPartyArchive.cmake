# third_party/ 归档缓存 + build/_deps/<package>-<version> 解压暂存
#
# 与 FetchContent 默认的 <name>-src 命名解耦：物理目录名对齐上游发布包解压结果
# （例如 GitHub paho.mqtt.c 的 v1.3.16 归档解压为 paho.mqtt.c-1.3.16/）。

if(NOT DEFINED OPENEMBER_THIRD_PARTY_CACHE_DIR)
    set(OPENEMBER_THIRD_PARTY_CACHE_DIR "${CMAKE_SOURCE_DIR}/third_party" CACHE PATH
        "Offline archive directory: place <package>-<version>.tar.gz|.zip here; FETCH downloads into this directory first.")
endif()

function(openember_third_party_cache_dir out_var)
    file(MAKE_DIRECTORY "${OPENEMBER_THIRD_PARTY_CACHE_DIR}")
    set(${out_var} "${OPENEMBER_THIRD_PARTY_CACHE_DIR}" PARENT_SCOPE)
endfunction()

# 在 cache 目录中查找 <pkg>-<ver>.tar.gz 或 .zip
function(openember_third_party_find_local_archive cache_dir pkg ver out_file)
    set(_tgz "${cache_dir}/${pkg}-${ver}.tar.gz")
    set(_zip "${cache_dir}/${pkg}-${ver}.zip")
    if(EXISTS "${_tgz}")
        set(${out_file} "${_tgz}" PARENT_SCOPE)
    elseif(EXISTS "${_zip}")
        set(${out_file} "${_zip}" PARENT_SCOPE)
    else()
        set(${out_file} "" PARENT_SCOPE)
    endif()
endfunction()

function(openember_third_party_extract_archive archive_path dest_parent)
    get_filename_component(_fname "${archive_path}" NAME)
    if(_fname MATCHES "\\.tar\\.gz$" OR _fname MATCHES "\\.tgz$")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${archive_path}"
            WORKING_DIRECTORY "${dest_parent}"
            RESULT_VARIABLE _rv
            OUTPUT_VARIABLE _out
            ERROR_VARIABLE _err)
        if(NOT _rv EQUAL 0)
            message(FATAL_ERROR "Extract failed (${archive_path}): ${_err}")
        endif()
        return()
    endif()
    if(_fname MATCHES "\\.zip$")
        find_program(_unzip NAMES unzip)
        if(_unzip)
            execute_process(
                COMMAND ${_unzip} -q -o "${archive_path}" -d "${dest_parent}"
                RESULT_VARIABLE _rv
                ERROR_VARIABLE _err)
            if(NOT _rv EQUAL 0)
                message(FATAL_ERROR "unzip failed (${archive_path}): ${_err}")
            endif()
        else()
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xf "${archive_path}"
                WORKING_DIRECTORY "${dest_parent}"
                RESULT_VARIABLE _rv
                ERROR_VARIABLE _err)
            if(NOT _rv EQUAL 0)
                message(FATAL_ERROR "Extract .zip failed (${archive_path}). Install unzip or use CMake 3.15+ tar. ${_err}")
            endif()
        endif()
        return()
    endif()
    message(FATAL_ERROR "Unsupported archive type: ${archive_path}")
endfunction()

# 将 URL 下载到 cache_dir 下固定文件名 <pkg>-<ver>.<ext>（与 URL 后缀一致）
function(openember_third_party_download_to_cache url cache_dir pkg ver out_file)
    if(url MATCHES "\\.zip($|\\?)")
        set(_dest "${cache_dir}/${pkg}-${ver}.zip")
    else()
        set(_dest "${cache_dir}/${pkg}-${ver}.tar.gz")
    endif()
    message(STATUS "Third-party: downloading ${pkg} ${ver} -> ${_dest}")
    file(DOWNLOAD "${url}" "${_dest}"
        TLS_VERIFY ON
        STATUS _st
        SHOW_PROGRESS)
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _st 1 _msg)
        message(FATAL_ERROR "Download failed (${url}): ${_code} ${_msg}")
    endif()
    set(${out_file} "${_dest}" PARENT_SCOPE)
endfunction()

function(openember_third_party_find_local_archive_by_key cache_dir cache_key out_file)
    set(_tgz "${cache_dir}/${cache_key}.tar.gz")
    set(_zip "${cache_dir}/${cache_key}.zip")
    if(EXISTS "${_tgz}")
        set(${out_file} "${_tgz}" PARENT_SCOPE)
    elseif(EXISTS "${_zip}")
        set(${out_file} "${_zip}" PARENT_SCOPE)
    else()
        set(${out_file} "" PARENT_SCOPE)
    endif()
endfunction()

function(openember_third_party_download_to_cache_key url cache_dir cache_key out_file)
    if(url MATCHES "\\.zip($|\\?)")
        set(_dest "${cache_dir}/${cache_key}.zip")
    else()
        set(_dest "${cache_dir}/${cache_key}.tar.gz")
    endif()
    message(STATUS "Third-party: downloading ${cache_key} -> ${_dest}")
    file(DOWNLOAD "${url}" "${_dest}"
        TLS_VERIFY ON
        STATUS _st
        SHOW_PROGRESS)
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _st 1 _msg)
        message(FATAL_ERROR "Download failed (${url}): ${_code} ${_msg}")
    endif()
    set(${out_file} "${_dest}" PARENT_SCOPE)
endfunction()

# openember_third_party_prepare_stage(out_source_dir cache_key stage_dir_name url marker_file [local_source])
# marker_file：相对源码根的路径，用于判断解压是否成功（如 CMakeLists.txt、src/zlog.h）。
function(openember_third_party_prepare_stage _out _cache_key _stage_dir_name _url _marker)
    set(_local "")
    if(${ARGC} GREATER 5)
        set(_local "${ARGV5}")
    endif()
    if(_local)
        if(NOT EXISTS "${_local}")
            message(FATAL_ERROR "Local source path does not exist: ${_local}")
        endif()
        if(NOT EXISTS "${_local}/${_marker}")
            message(FATAL_ERROR "Expected ${_marker} under ${_local}")
        endif()
        set(${_out} "${_local}" PARENT_SCOPE)
        return()
    endif()

    set(_deps_parent "${CMAKE_BINARY_DIR}/_deps")
    set(_stage "${_deps_parent}/${_stage_dir_name}")
    if(EXISTS "${_stage}/${_marker}")
        set(${_out} "${_stage}" PARENT_SCOPE)
        return()
    endif()

    file(MAKE_DIRECTORY "${_deps_parent}")
    openember_third_party_cache_dir(_cache)
    openember_third_party_find_local_archive_by_key("${_cache}" "${_cache_key}" _archive)

    if(NOT _archive)
        if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH")
            openember_third_party_download_to_cache_key("${_url}" "${_cache}" "${_cache_key}" _archive)
        elseif(OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
            message(FATAL_ERROR
                "Third-party (VENDOR): place ${_cache_key}.tar.gz or ${_cache_key}.zip in ${_cache}, "
                "or set the corresponding *_LOCAL_SOURCE cache to an extracted tree.")
        else()
            message(FATAL_ERROR "Third-party: no archive in ${_cache} for ${_cache_key} and no local source set.")
        endif()
    endif()

    message(STATUS "Third-party: extracting ${_archive} -> ${_deps_parent}")
    openember_third_party_extract_archive("${_archive}" "${_deps_parent}")
    if(NOT EXISTS "${_stage}/${_marker}")
        message(FATAL_ERROR
            "After extract, expected ${_stage}/${_marker} (check URL / version pins in Dependencies.cmake).")
    endif()
    set(${_out} "${_stage}" PARENT_SCOPE)
endfunction()
