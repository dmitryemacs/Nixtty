cmake_minimum_required(VERSION 3.16)
project(nixtty LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(COMMON_SOURCES
    src/main.cpp
    src/terminal.cpp
    src/renderer.cpp
    src/ansi.cpp
    src/config.cpp
)

if(APPLE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    find_package(OpenGL REQUIRED)
    find_library(CORETEXT CoreText REQUIRED)
    find_library(COCOA Cocoa REQUIRED)
    find_library(IOKIT IOKit REQUIRED)
    find_library(COREVIDEO CoreVideo REQUIRED)

    add_executable(nixtty MACOSX_BUNDLE
        ${COMMON_SOURCES}
        src/pty_posix.cpp
    )
    target_include_directories(nixtty PRIVATE include)
    target_link_libraries(nixtty PRIVATE
        glfw
        OpenGL::GL
        ${CORETEXT}
        ${COCOA}
        ${IOKIT}
        ${COREVIDEO}
    )
elseif(WIN32)
    add_executable(nixtty WIN32
        ${COMMON_SOURCES}
        src/pty.cpp
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
else()
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    find_package(OpenGL REQUIRED)

    add_executable(nixtty
        ${COMMON_SOURCES}
        src/pty_posix.cpp
    )
    target_include_directories(nixtty PRIVATE include)
    target_link_libraries(nixtty PRIVATE
        glfw
        OpenGL::GL
        util
    )
endif()
