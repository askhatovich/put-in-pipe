cmake_minimum_required(VERSION 3.16)

project(put-in-pipe LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Version: from APP_VERSION env (CI tag), or git short hash
if(DEFINED ENV{APP_VERSION})
    set(APP_VERSION "$ENV{APP_VERSION}")
else()
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_SHORT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_SHORT_HASH)
        set(APP_VERSION "${GIT_SHORT_HASH}")
    else()
        set(APP_VERSION "dev")
    endif()
endif()
message(STATUS "App version: ${APP_VERSION}")

# Core library (shared between main executable and tests)
add_library(put-in-pipe-core STATIC
    transfersession.cpp
    buffer.cpp
    chunk.cpp
    client.cpp
    clientlist.cpp
    captcha/skaptcha_backend/captcha.c
    captcha/skaptcha_backend/lodepng.c
    captcha/skaptcha_backend/pictures.cpp
    captcha/skaptcha.cpp
    config/ini_backend/ini.c
    config/config.cpp
    config/inireader.cpp
    captcha/skaptcha_tools.cpp
    captcha/chacha20_backend/portable8439.c
    captcha/sha256_backend/sha256.cpp
    websocketconnection.cpp
    serializableevent.cpp
    transfersessionlist.cpp
    timercallback.cpp
    webapi.cpp
    captcha/token.cpp

    # Headers (for IDE visibility)
    atomicset.h
    buffer.h
    chunk.h
    client.h
    clientlist.h
    log.h
    observerpattern.h
    serializableevent.h
    timercallback.h
    transfersession.h
    transfersessionlist.h
    webapi.h
    websocketconnection.h
    config/config.h
    config/inireader.h
    captcha/skaptcha.h
    captcha/skaptcha_tools.h
    captcha/token.h
)

target_include_directories(put-in-pipe-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(put-in-pipe-core PUBLIC CROW_ENFORCE_WS_SPEC APP_VERSION="${APP_VERSION}")

# Main executable
add_executable(put-in-pipe main.cpp)
target_link_libraries(put-in-pipe PRIVATE put-in-pipe-core)

include(GNUInstallDirs)
install(TARGETS put-in-pipe
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
