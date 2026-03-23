# Standalone Asio（header-only），不依赖 Boost
include(FetchContent)

# 上游 tag 形如 asio-1-30-2（连字符，非 1.30.2）
set(OPENEMBER_ASIO_TAG "asio-1-30-2" CACHE STRING "Asio Git tag (e.g. asio-1-30-2)")
set(OPENEMBER_ASIO_URL
    "https://github.com/chriskohlhoff/asio/archive/${OPENEMBER_ASIO_TAG}.tar.gz"
    CACHE STRING "Asio source archive URL")

function(openember_get_asio)
    if(OPENEMBER_ASIO_LOCAL_SOURCE)
        FetchContent_Declare(
            openember_asio_fc
            SOURCE_DIR "${OPENEMBER_ASIO_LOCAL_SOURCE}")
    else()
        FetchContent_Declare(
            openember_asio_fc
            URL ${OPENEMBER_ASIO_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()

    FetchContent_GetProperties(openember_asio_fc)
    if(NOT openember_asio_fc_POPULATED)
        FetchContent_Populate(openember_asio_fc)
    endif()

    if(NOT TARGET openember::asio)
        add_library(openember_asio INTERFACE)
        add_library(openember::asio ALIAS openember_asio)
        target_include_directories(openember_asio INTERFACE
            $<BUILD_INTERFACE:${openember_asio_fc_SOURCE_DIR}/asio/include>)
        target_compile_definitions(openember_asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
        find_package(Threads REQUIRED)
        target_link_libraries(openember_asio INTERFACE Threads::Threads)
    endif()
endfunction()
