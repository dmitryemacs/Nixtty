# Nixtty

GPU-accelerated terminal emulator for Windows, built with C++17, OpenGL, and ConPTY.

![Windows](https://img.shields.io/badge/Platform-Windows-blue) ![C++17](https://img.shields.io/badge/C%2B%2B-17-blue) ![License](https://img.shields.io/badge/License-MIT-green)

## Features

- **ConPTY** — native Windows pseudo-console integration (PowerShell, cmd.exe)
- **OpenGL 1.1** rendering via fixed-function pipeline (no shaders)
- **Nerd Fonts** — Hack Nerd Font Mono with 5000+ glyphs (icons, box drawing, powerline)
- **Full Unicode** — UTF-8 input/output with Cyrillic and multibyte character support
- **ANSI escape sequences** — CSI, SGR (16/256/TrueColor), cursor movement, scroll regions, erase operations
- **Dark theme** — Tokyo Night color palette
- **Window management** — resize, minimize, dark title bar (DwmSetWindowAttribute)

## Requirements

- Windows 10 1809+ (for ConPTY)
- MinGW-w64 with GCC 13+
- CMake 3.16+
- Hack Nerd Font Mono installed on the system

## Build

```bash
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## Architecture

```
src/
  main.cpp       Win32 window, message loop, keyboard input, PTY bridge
  terminal.cpp   Cell-based terminal buffer, cursor, scroll
  ansi.cpp       ANSI escape sequence parser with UTF-8 decoder
  pty.cpp        ConPTY wrapper (spawn, read thread, resize)
  renderer.cpp   OpenGL 1.1 renderer with GDI font atlas

include/
  terminal.h     Terminal/Cell/Cursor structs
  ansi.h         AnsiParser class
  pty.h          Pty class
  renderer.h     Renderer class with glyph atlas
```

### Data flow

```
Keyboard input -> WM_CHAR -> UTF-8 encode -> PTY write pipe
                                                    |
                                              ConPTY shell
                                                    |
PTY read pipe -> UTF-8 decode -> ANSI parse -> Terminal buffer
                                                    |
                                              OpenGL render
                                                    |
                                                 Screen
```
