cmake_minimum_required(VERSION 3.16)
project(remotecontrol-desktop-windows C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Windows-specific flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Find vcpkg or manual libraries
find_package(CURL REQUIRED)
find_package(jansson CONFIG QUIET)

set(SOURCES
    main.c
    input_win32.c
    volume_win32.c
    client_net.c
    login_window.c
    tray.c
)

add_executable(remotecontrol-desktop WIN32 ${SOURCES})

target_include_directories(remotecontrol-desktop PRIVATE
    ${CMAKE_SOURCE_DIR}/../shared
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(remotecontrol-desktop
    ${CURL_LIBRARIES}
    jansson
    ws2_32
    winmm
    shell32
    user32
    ole32
    comctl32
)
