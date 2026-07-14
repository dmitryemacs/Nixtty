#pragma once

#include <string>
#include <cstdint>

struct Config {
    // Font
    std::string fontFamily = "Hack Nerd Font Mono";
    int fontSize = 16;

    // Colors
    uint32_t background = 0x1A1B26;
    uint32_t foreground = 0xC0CAF5;
    uint32_t cursor = 0xFFFFFF;
    uint32_t selection = 0x4D6299;

    // ANSI colors (0-15)
    uint32_t ansiColors[16] = {
        0x1A1B26, // 0: Black
        0xF7768E, // 1: Red
        0x9ECE6A, // 2: Green
        0xE0AF68, // 3: Yellow
        0x7AA2F7, // 4: Blue
        0xBB9AF7, // 5: Magenta
        0x7DCFFF, // 6: Cyan
        0xA9B1D6, // 7: White
        0x414868, // 8: Bright Black
        0xF7768E, // 9: Bright Red
        0x9ECE6A, // 10: Bright Green
        0xE0AF68, // 11: Bright Yellow
        0x7AA2F7, // 12: Bright Blue
        0xBB9AF7, // 13: Bright Magenta
        0x7DCFFF, // 14: Bright Cyan
        0xC0CAF5, // 15: Bright White
    };

    // Window
    float opacity = 1.0f;

    bool loadFromFile(const std::string& path);
};
