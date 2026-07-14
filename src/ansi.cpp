#include "ansi.h"
#include <algorithm>

AnsiParser::AnsiParser(Terminal& terminal)
    : m_terminal(terminal)
{
}

void AnsiParser::parse(const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = static_cast<unsigned char>(data[i]);

        if (m_utf8Expected > 0) {
            if ((c & 0xC0) == 0x80) {
                m_utf8Codepoint = (m_utf8Codepoint << 6) | (c & 0x3F);
                m_utf8Expected--;
                if (m_utf8Expected == 0) {
                    processChar(static_cast<wchar_t>(m_utf8Codepoint));
                }
            } else {
                m_utf8Expected = 0;
                i--;
            }
        } else if (c < 0x80) {
            processChar(static_cast<wchar_t>(c));
        } else if ((c & 0xE0) == 0xC0) {
            m_utf8Expected = 1;
            m_utf8Codepoint = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            m_utf8Expected = 2;
            m_utf8Codepoint = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            m_utf8Expected = 3;
            m_utf8Codepoint = c & 0x07;
        }
    }
}

void AnsiParser::parse(const wchar_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        processChar(data[i]);
    }
}

void AnsiParser::processChar(wchar_t ch) {
    switch (m_state) {
    case STATE_GROUND:
        if (ch == L'\x1b') {
            m_state = STATE_ESC;
            m_sequence.clear();
            m_params.clear();
            m_currentParam = 0;
            m_hasParam = false;
        } else {
            // Regular character - pass through to terminal
            m_terminal.putChar(ch);
        }
        break;

    case STATE_ESC:
        if (ch == L'[' || ch == L's') {
            m_state = STATE_CSI;
            m_sequence.clear();
            m_params.clear();
            m_currentParam = 0;
            m_hasParam = false;
        } else if (ch == L']') {
            m_state = STATE_OSC;
            m_sequence.clear();
        } else if (ch == L'7') {
            m_state = STATE_GROUND;
        } else if (ch == L'8') {
            m_state = STATE_GROUND;
        } else if (ch == L'D') {
            m_terminal.moveCursor(0, 1);
            m_state = STATE_GROUND;
        } else if (ch == L'M') {
            m_terminal.moveCursor(0, -1);
            m_state = STATE_GROUND;
        } else if (ch == L'c') {
            m_terminal.resetAttributes();
            m_terminal.eraseScreen();
            m_terminal.setCursorPos(0, 0);
            m_state = STATE_GROUND;
        } else if (ch == L'(' || ch == L')') {
            m_state = STATE_ESC_IGNORE;
        } else if (ch >= L'0' && ch <= L'~') {
            m_state = STATE_GROUND;
        } else {
            m_state = STATE_GROUND;
        }
        break;

    case STATE_CSI:
        if (ch >= L'0' && ch <= L'9') {
            m_currentParam = m_currentParam * 10 + (ch - L'0');
            m_hasParam = true;
        } else if (ch == L';') {
            m_params.push_back(m_currentParam);
            m_currentParam = 0;
            m_hasParam = false;
        } else if (ch == L'?' || ch == L'>' || ch == L'=') {
            // Intermediate byte (DEC private, etc.) - ignore and continue parsing
        } else if (ch >= L'@' && ch <= L'~') {
            if (m_hasParam || !m_params.empty()) {
                m_params.push_back(m_currentParam);
            }
            m_sequence.push_back(ch);
            executeCsi();
            m_state = STATE_GROUND;
        } else {
            m_state = STATE_GROUND;
        }
        break;

    case STATE_OSC:
        // Skip OSC sequences (just consume until BEL or ST)
        if (ch == L'\x07' || ch == L'\x1b') {
            m_state = STATE_GROUND;
        }
        break;

    case STATE_ESC_IGNORE:
        // Consume one character after ESC ( or ESC ) then return to ground
        m_state = STATE_GROUND;
        break;
    }
}

void AnsiParser::executeCsi() {
    if (m_sequence.empty()) return;

    wchar_t finalChar = m_sequence.back();

    switch (finalChar) {
    case L'A': { // Cursor Up
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.moveCursor(0, -n);
        break;
    }
    case L'B': { // Cursor Down
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.moveCursor(0, n);
        break;
    }
    case L'C': { // Cursor Forward
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.moveCursor(n, 0);
        break;
    }
    case L'D': { // Cursor Back
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.moveCursor(-n, 0);
        break;
    }
    case L'E': { // Cursor Next Line
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.setCursorPos(0, m_terminal.getCursor().y + n);
        break;
    }
    case L'F': { // Cursor Previous Line
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.setCursorPos(0, m_terminal.getCursor().y - n);
        break;
    }
    case L'G': { // Cursor Horizontal Absolute
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.setCursorPos(n - 1, m_terminal.getCursor().y);
        break;
    }
    case L'H': // Cursor Position
    case L'f': {
        int row = m_params.size() >= 1 ? m_params[0] : 1;
        int col = m_params.size() >= 2 ? m_params[1] : 1;
        m_terminal.setCursorPos(col - 1, row - 1);
        break;
    }
    case L'J': { // Erase in Display
        int n = m_params.empty() ? 0 : m_params[0];
        switch (n) {
        case 0: m_terminal.eraseToEndOfScreen(); break;
        case 1: m_terminal.eraseToStartOfScreen(); break;
        case 2: m_terminal.eraseScreen(); break;
        case 3: m_terminal.eraseScreen(); break;
        }
        break;
    }
    case L'K': { // Erase in Line
        int n = m_params.empty() ? 0 : m_params[0];
        switch (n) {
        case 0: m_terminal.eraseToEndOfLine(); break;
        case 1: m_terminal.eraseToStartOfLine(); break;
        case 2: m_terminal.eraseLine(); break;
        }
        break;
    }
    case L'L': { // Insert Lines
        // Not implemented yet
        break;
    }
    case L'M': { // Delete Lines
        // Not implemented yet
        break;
    }
    case L'P': { // Delete Characters
        // Not implemented yet
        break;
    }
    case L'X': { // Erase Characters
        // Not implemented yet
        break;
    }
    case L'S': { // Scroll Up
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.scrollUp(n);
        break;
    }
    case L'T': { // Scroll Down
        // Not implemented yet
        break;
    }
    case L'@': { // Insert Characters
        // Not implemented yet
        break;
    }
    case L'm': { // SGR - Select Graphic Rendition
        executeSgr();
        break;
    }
    case L'r': { // Set Scroll Region
        int top = m_params.size() >= 1 ? m_params[0] - 1 : 0;
        int bottom = m_params.size() >= 2 ? m_params[1] - 1 : m_terminal.getRows() - 1;
        m_terminal.setScrollRegion(top, bottom);
        break;
    }
    case L'h': { // Set Mode
        // Ignore for now (DECSET)
        break;
    }
    case L'l': { // Reset Mode
        // Ignore for now (DECRST)
        break;
    }
    case L'n': { // Device Status Report
        // Not implemented yet
        break;
    }
    }
}

// Standard ANSI 16 colors
static const uint32_t ANSI_COLORS[] = {
    0x1A1B26, // Black
    0xF7768E, // Red
    0x9ECE6A, // Green
    0xE0AF68, // Yellow
    0x7AA2F7, // Blue
    0xBB9AF7, // Magenta
    0x7DCFFF, // Cyan
    0xA9B1D6, // White
    0x414868, // Bright Black
    0xF7768E, // Bright Red
    0x9ECE6A, // Bright Green
    0xE0AF68, // Bright Yellow
    0x7AA2F7, // Bright Blue
    0xBB9AF7, // Bright Magenta
    0x7DCFFF, // Bright Cyan
    0xC0CAF5, // Bright White
};

void AnsiParser::executeSgr() {
    if (m_params.empty()) {
        m_terminal.resetAttributes();
        return;
    }

    size_t i = 0;
    while (i < m_params.size()) {
        int code = m_params[i];

        if (code == 0) {
            m_terminal.resetAttributes();
        } else if (code == 1) {
            m_terminal.setBold(true);
        } else if (code == 2) {
            // Faint - treat as reset bold
            m_terminal.setBold(false);
        } else if (code == 7) {
            // Inverse - TODO
        } else if (code == 22) {
            m_terminal.setBold(false);
        } else if (code >= 30 && code <= 37) {
            m_terminal.setFgColor(ANSI_COLORS[code - 30]);
        } else if (code == 38) {
            // Extended foreground color
            if (i + 1 < m_params.size()) {
                if (m_params[i + 1] == 5 && i + 2 < m_params.size()) {
                    // 256-color: \e[38;5;Nm
                    int colorIdx = m_params[i + 2];
                    if (colorIdx < 16) {
                        m_terminal.setFgColor(ANSI_COLORS[colorIdx]);
                    } else if (colorIdx < 232) {
                        // 6x6x6 color cube
                        int idx = colorIdx - 16;
                        int b = idx % 6; idx /= 6;
                        int g = idx % 6; idx /= 6;
                        int r = idx;
                        m_terminal.setFgColor(
                            (r == 0 ? 0x55 : r * 40 + 55) << 16 |
                            (g == 0 ? 0x55 : g * 40 + 55) << 8 |
                            (b == 0 ? 0x55 : b * 40 + 55)
                        );
                    } else if (colorIdx < 256) {
                        // Grayscale ramp
                        int gray = (colorIdx - 232) * 10 + 8;
                        m_terminal.setFgColor((gray << 16) | (gray << 8) | gray);
                    }
                    i += 2;
                } else if (m_params[i + 1] == 2 && i + 3 < m_params.size()) {
                    // True color: \e[38;2;R;G;Bm
                    uint32_t r = m_params[i + 2];
                    uint32_t g = m_params[i + 3];
                    uint32_t b = m_params[i + 4];
                    m_terminal.setFgColor((r << 16) | (g << 8) | b);
                    i += 4;
                }
            }
        } else if (code == 39) {
            m_terminal.setFgColor(0xA9B1D6); // Default fg
        } else if (code >= 40 && code <= 47) {
            m_terminal.setBgColor(ANSI_COLORS[code - 40]);
        } else if (code == 48) {
            // Extended background color
            if (i + 1 < m_params.size()) {
                if (m_params[i + 1] == 5 && i + 2 < m_params.size()) {
                    int colorIdx = m_params[i + 2];
                    if (colorIdx < 16) {
                        m_terminal.setBgColor(ANSI_COLORS[colorIdx]);
                    } else if (colorIdx < 232) {
                        int idx = colorIdx - 16;
                        int b = idx % 6; idx /= 6;
                        int g = idx % 6; idx /= 6;
                        int r = idx;
                        m_terminal.setBgColor(
                            (r == 0 ? 0x55 : r * 40 + 55) << 16 |
                            (g == 0 ? 0x55 : g * 40 + 55) << 8 |
                            (b == 0 ? 0x55 : b * 40 + 55)
                        );
                    } else if (colorIdx < 256) {
                        int gray = (colorIdx - 232) * 10 + 8;
                        m_terminal.setBgColor((gray << 16) | (gray << 8) | gray);
                    }
                    i += 2;
                } else if (m_params[i + 1] == 2 && i + 3 < m_params.size()) {
                    uint32_t r = m_params[i + 2];
                    uint32_t g = m_params[i + 3];
                    uint32_t b = m_params[i + 4];
                    m_terminal.setBgColor((r << 16) | (g << 8) | b);
                    i += 4;
                }
            }
        } else if (code == 49) {
            m_terminal.setBgColor(0x1A1B26); // Default bg
        }

        i++;
    }
}
