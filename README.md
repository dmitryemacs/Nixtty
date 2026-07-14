# Nixtty

GPU-accelerated terminal emulator with cross-platform support, built with C++17, OpenGL, and ConPTY/Core Text.

![Windows](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-blue) ![C++17](https://img.shields.io/badge/C%2B%2B-17-blue) ![License](https://img.shields.io/badge/License-MIT-green)

## Features

- **Cross-platform** — Windows (ConPTY), macOS (Core Text + POSIX PTY), Linux
- **OpenGL 1.1** rendering via fixed-function pipeline (no shaders)
- **Nerd Fonts** — Hack Nerd Font Mono with 6800+ glyphs (icons, box drawing, powerline, Cyrillic, Greek)
- **Full Unicode** — UTF-8 input/output with Cyrillic, multibyte character support
- **ANSI escape sequences** — CSI, SGR (16/256/TrueColor), cursor movement, scroll regions, erase operations
- **Italic text** — SGR 3/23 support
- **Scrollback buffer** — 10,000 lines with Shift+scroll navigation
- **Mouse tracking** — X10 (mode 1000), button-event (1002), SGR encoding (1006)
- **Bracketed paste** — mode 2004
- **Text selection** — click-drag, Ctrl+A select all, Ctrl+C copy to clipboard
- **Block cursor** — blinking at 1Hz, white
- **Dark theme** — Tokyo Night color palette
- **TOML config** — customizable fonts, colors, window opacity (`config.toml`)
- **Window management** — resize, minimize, dark title bar (DwmSetWindowAttribute on Windows)

## Requirements

### Windows
- Windows 10 1809+ (for ConPTY)
- MinGW-w64 with Clang 17+ or GCC 13+
- CMake 3.16+
- Hack Nerd Font Mono installed on the system

### macOS
- macOS 10.15+ (Catalina)
- Xcode command-line tools
- CMake 3.16+
- Hack Nerd Font Mono installed

## Build

### Windows
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### macOS
```bash
mkdir build && cd build
cmake .. -G "Unix Makefiles"
cmake --build .
```

## Configuration

Copy `config.toml` to `~/.config/nixtty/config.toml` and customize:

```toml
[font]
family = "Hack Nerd Font Mono"
size = 16

[colors]
background = "#1A1B26"
foreground = "#C0CAF5"
cursor = "#FFFFFF"
selection = "#4D6299"

# ANSI colors (0-15)
color0 = "#1A1B26"    # Black
color1 = "#F7768E"    # Red
color2 = "#9ECE6A"    # Green
color3 = "#E0AF68"    # Yellow
color4 = "#7AA2F7"    # Blue
color5 = "#BB9AF7"    # Magenta
color6 = "#7DCFFF"    # Cyan
color7 = "#A9B1D6"    # White

[window]
opacity = 1.0
```

## Architecture

```
src/
  main.cpp         Platform entry point, window, input handling, PTY bridge
  terminal.cpp     Cell-based terminal buffer, cursor, scroll, scrollback
  ansi.cpp         ANSI escape sequence parser with UTF-8 decoder
  pty.cpp          Windows ConPTY wrapper (spawn, read thread, resize)
  pty_posix.cpp    macOS/Linux POSIX PTY wrapper
  renderer.cpp     OpenGL renderer with GDI/Core Text font atlas
  config.cpp       TOML config file parser

include/
  terminal.h       Terminal/Cell/Cursor structs, modes, scrollback
  ansi.h           AnsiParser class
  pty.h            Pty class (platform interface)
  renderer.h       Renderer class with glyph atlas
  config.h         Config struct
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

### Branches

| Branch | Description |
|--------|-------------|
| `main` | Core platform-independent code |
| `macos` | macOS port (GLFW + Core Text) |
| `windows` | Windows build (Win32 API + ConPTY) |
