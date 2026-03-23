cmake_minimum_required(VERSION 3.16)

project(put-in-pipe LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
)

target_include_directories(put-in-pipe-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(put-in-pipe-core PUBLIC CROW_ENFORCE_WS_SPEC)

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
