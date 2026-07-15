cmake_minimum_required(VERSION 3.16)
project(nixtty LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(nixtty WIN32
    src/main.cpp
    src/terminal.cpp
    src/renderer.cpp
    src/pty.cpp
    src/ansi.cpp
    src/unicode.cpp
)

target_include_directories(nixtty PRIVATE include)

target_link_libraries(nixtty PRIVATE
    opengl32
    gdi32
    kernel32
    user32
    dwmapi
    imm32
)

target_link_options(nixtty PRIVATE -municode -mwindows)

target_compile_definitions(nixtty PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set(WINDRES "D:/Qt/Tools/llvm-mingw1706_64/bin/x86_64-w64-mingw32-windres.exe")

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/app.res.o
    COMMAND ${WINDRES} -I${CMAKE_CURRENT_SOURCE_DIR} -o ${CMAKE_CURRENT_BINARY_DIR}/app.res.o ${CMAKE_CURRENT_SOURCE_DIR}/app.rc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app.rc ${CMAKE_CURRENT_SOURCE_DIR}/terminal.ico
    COMMENT "Compiling Windows resources"
)

add_custom_target(resource_file DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/app.res.o)
add_dependencies(nixtty resource_file)

target_link_libraries(nixtty PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.res.o)
