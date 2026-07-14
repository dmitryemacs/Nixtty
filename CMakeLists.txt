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
