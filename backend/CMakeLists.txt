cmake_minimum_required(VERSION 3.16)
project(remotecontrol-server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")

# Find required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(MICROHTTPD REQUIRED libmicrohttpd)
pkg_check_modules(LIBPQ REQUIRED libpq)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(JANSSON REQUIRED jansson)
pkg_check_modules(LIBCURL REQUIRED libcurl)

# Source files
set(SOURCES
    src/main.c
    src/config.c
    src/http_server.c
    src/http_routes.c
    src/auth.c
    src/jwt.c
    src/database.c
    src/migrations.c
    src/email.c
    src/socket_server.c
    src/protocol.c
    src/session_manager.c
    src/admin_panel.c
    src/utils.c
)

add_executable(remotecontrol-server ${SOURCES})

target_include_directories(remotecontrol-server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${MICROHTTPD_INCLUDE_DIRS}
    ${LIBPQ_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${JANSSON_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
)

target_link_libraries(remotecontrol-server
    ${MICROHTTPD_LIBRARIES}
    ${LIBPQ_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${JANSSON_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    pthread
)
