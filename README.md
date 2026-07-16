# Nixtty

GPU-accelerated terminal emulator with cross-platform support, built with C++17, OpenGL, and ConPTY/Core Text.

![Windows](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-blue) ![C++17](https://img.shields.io/badge/C%2B%2B-17-blue) ![License](https://img.shields.io/badge/License-MIT-green)

## Features

- **Cross-platform** — Windows (ConPTY), macOS (Core Text + POSIX PTY), Linux
- **OpenGL 1.1** rendering via fixed-function pipeline (no shaders)
- **Nerd Fonts** — Hack Nerd Font with 6800+ glyphs (icons, box drawing, powerline, Cyrillic, Greek)
- **Full Unicode** — UTF-8 input/output with Cyrillic, multibyte character support
- **ANSI escape sequences** — CSI, SGR (16/256/TrueColor), cursor movement, scroll regions, erase operations
- **Bright colors** — SGR 90-97 (bright foreground), 100-107 (bright background)
- **DA1/DA2 responses** — Device Attribute queries answered (VT100 compatible)
- **DSR responses** — Device Status Report (operating status, cursor position)
- **DCS/APC/SOS/PM handling** — String payloads consumed without leaking to screen
- **Italic text** — SGR 3/23 support
- **Scrollback buffer** — configurable (default 10,000 lines) with Shift+scroll navigation
- **Mouse tracking** — X10 (mode 1000), button-event (1002), SGR encoding (1006)
- **Bracketed paste** — mode 2004
- **Text selection** — click-drag, Ctrl+A select all, Ctrl+C copy to clipboard
- **Block cursor** — configurable blink rate and on/off
- **Dark theme** — Tango high-contrast color palette (16 ANSI colors)
- **TOML config** — customizable fonts, colors, window opacity, terminal size (`config.toml`)
- **Window management** — resize, minimize, dark title bar (DwmSetWindowAttribute on Windows)

## Requirements

### Windows
- Windows 10 1809+ (for ConPTY)
- MinGW-w64 with Clang 17+ or GCC 13+
- CMake 3.16+
- Hack Nerd Font installed on the system

### macOS
- macOS 10.15+ (Catalina)
- Xcode command-line tools
- CMake 3.16+
- Hack Nerd Font installed

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

### Tests
```bash
cmake --build build --target nixtty_tests
build/nixtty_tests
```

## Configuration

Copy `config.toml` to `~/.config/nixtty/config.toml` and customize:

```toml
[font]
family = "Hack Nerd Font"
size = 16

[colors]
background = "#1E1E1E"
foreground = "#FFFFFF"
cursor = "#FFFFFF"
selection = "#404040"

# ANSI colors (0-15) — Tango palette
color0 = "#000000"    # Black
color1 = "#CC0000"    # Red
color2 = "#4E9A06"    # Green
color3 = "#C4A000"    # Yellow
color4 = "#3465A4"    # Blue
color5 = "#75507B"    # Magenta
color6 = "#06989A"    # Cyan
color7 = "#D3D7CF"    # White
color8 = "#555753"    # Bright Black
color9 = "#EF2929"    # Bright Red
color10 = "#8AE234"   # Bright Green
color11 = "#FCE94F"   # Bright Yellow
color12 = "#729FCF"   # Bright Blue
color13 = "#AD7FA8"   # Bright Magenta
color14 = "#34E2E2"   # Bright Cyan
color15 = "#EEEEEC"   # Bright White

[window]
opacity = 1.0

[terminal]
cols = 100
rows = 30
scrollback = 10000
cursor_blink = true
cursor_blink_ms = 500
```

## Architecture

```
src/
  main.cpp         Platform entry point, window, input handling, PTY bridge
  terminal.cpp     Cell-based terminal buffer, cursor, scroll, scrollback
  ansi.cpp         ANSI escape sequence parser with UTF-8 decoder, DA/DSR/OSC responses
  pty.cpp          Windows ConPTY wrapper (spawn, read thread, resize)
  pty_posix.cpp    macOS/Linux POSIX PTY wrapper
  renderer.cpp     OpenGL renderer with GDI/Core Text font atlas
  config.cpp       TOML config file parser

include/
  terminal.h       Terminal/Cell/Cursor structs, modes, scrollback
  ansi.h           AnsiParser class with DCS/CSI/OSC states
  pty.h            Pty class (platform interface)
  renderer.h       Renderer class with glyph atlas
  config.h         Config struct

tests/
  test_ansi.cpp    ANSI parser tests (137 tests total)
  test_terminal.cpp  Terminal buffer and operation tests
  test_config.cpp  Config parsing tests
  test_unicode.cpp Unicode width and grapheme tests
```

### Data flow

```
Keyboard input -> WM_CHAR -> UTF-8 encode -> PTY write pipe
                                                    |
                                              ConPTY shell
                                                    |
PTY read pipe -> UTF-8 decode -> ANSI parse -> Terminal buffer
                          |                        |
                    writeResponse              OpenGL render
                          |                        |
                    PTY write pipe              Screen
                   (DA/DSR/OSC responses back to shell)
```
