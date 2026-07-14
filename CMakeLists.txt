cmake_minimum_required(VERSION 3.16)
project(winterm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(winterm WIN32
    src/main.cpp
    src/terminal.cpp
    src/renderer.cpp
    src/pty.cpp
    src/ansi.cpp
)

target_include_directories(winterm PRIVATE include)

target_link_libraries(winterm PRIVATE
    opengl32
    gdi32
    kernel32
    user32
    dwmapi
)

target_link_options(winterm PRIVATE -municode -mwindows)

target_compile_definitions(winterm PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
