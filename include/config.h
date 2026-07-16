#pragma once

#include <string>
#include <cstdint>

struct Config {
    // Font
    std::string fontFamily = "Hack Nerd Font";
    int fontSize = 16;

    // Colors
    uint32_t background = 0x1E1E1E;
    uint32_t foreground = 0xFFFFFF;
    uint32_t cursor = 0xFFFFFF;
    uint32_t selection = 0x404040;

    // ANSI colors (0-15)
    uint32_t ansiColors[16] = {
        0x000000, //  0: Black
        0xCC0000, //  1: Red
        0x4E9A06, //  2: Green
        0xC4A000, //  3: Yellow
        0x3465A4, //  4: Blue
        0x75507B, //  5: Magenta
        0x06989A, //  6: Cyan
        0xD3D7CF, //  7: White
        0x555753, //  8: Bright Black
        0xEF2929, //  9: Bright Red
        0x8AE234, // 10: Bright Green
        0xFCE94F, // 11: Bright Yellow
        0x729FCF, // 12: Bright Blue
        0xAD7FA8, // 13: Bright Magenta
        0x34E2E2, // 14: Bright Cyan
        0xEEEEEC, // 15: Bright White
    };

    // Window
    float opacity = 1.0f;

    // Terminal
    int cols = 100;
    int rows = 30;
    int scrollback = 10000;
    bool cursorBlink = true;
    int cursorBlinkMs = 500;

    bool loadFromFile(const std::string& path);
};
