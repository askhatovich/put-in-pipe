cmake_minimum_required(VERSION 3.16)

project(put-in-pipe LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(put-in-pipe main.cpp
    transfersession.h transfersession.cpp
    buffer.h buffer.cpp
    chunk.h chunk.cpp
    client.h client.cpp
    clientlist.h clientlist.cpp
    captcha/skaptcha_backend/captcha.c
    captcha/skaptcha_backend/captcha.h
    captcha/skaptcha_backend/lodepng.c
    captcha/skaptcha_backend/lodepng.h
    captcha/skaptcha_backend/pictures.cpp
    captcha/skaptcha.cpp
    captcha/skaptcha.h
    config/ini_backend/ini.c
    config/ini_backend/ini.h
    config/config.cpp
    config/config.h
    config/inireader.cpp
    config/inireader.h
    crowlib/crow/middlewares/cookie_parser.h
    crowlib/crow/middlewares/cors.h
    crowlib/crow/middlewares/session.h
    crowlib/crow/middlewares/utf-8.h
    crowlib/crow/app.h
    crowlib/crow/ci_map.h
    crowlib/crow/common.h
    crowlib/crow/compression.h
    crowlib/crow/exceptions.h
    crowlib/crow/http_connection.h
    crowlib/crow/http_parser_merged.h
    crowlib/crow/http_request.h
    crowlib/crow/http_response.h
    crowlib/crow/http_server.h
    crowlib/crow/json.h
    crowlib/crow/logging.h
    crowlib/crow/middleware.h
    crowlib/crow/middleware_context.h
    crowlib/crow/mime_types.h
    crowlib/crow/multipart.h
    crowlib/crow/multipart_view.h
    crowlib/crow/mustache.h
    crowlib/crow/parser.h
    crowlib/crow/query_string.h
    crowlib/crow/returnable.h
    crowlib/crow/routing.h
    crowlib/crow/settings.h
    crowlib/crow/socket_adaptors.h
    crowlib/crow/task_timer.h
    crowlib/crow/TinySHA1.hpp
    crowlib/crow/utility.h
    crowlib/crow/version.h
    crowlib/crow/websocket.h
    crowlib/crow.h
    captcha/token.h
    captcha/skaptcha_tools.h captcha/skaptcha_tools.cpp
    captcha/chacha20_backend/portable8439.c captcha/chacha20_backend/portable8439.h
    captcha/sha256_backend/sha256.cpp captcha/sha256_backend/sha256.h
    observerpattern.h
    websocketconnection.h websocketconnection.cpp
    serializableevent.h serializableevent.cpp
    transfersessionlist.h transfersessionlist.cpp
    timercallback.h timercallback.cpp
    webapi.h webapi.cpp
    captcha/token.cpp
)

target_compile_definitions(put-in-pipe PRIVATE CROW_ENFORCE_WS_SPEC)

include(GNUInstallDirs)
install(TARGETS put-in-pipe
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
