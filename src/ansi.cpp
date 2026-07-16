#include "ansi.h"
#include <algorithm>
#include <cstdio>

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
                    if (m_utf8Codepoint >= 0x10000) {
                        uint32_t cp = m_utf8Codepoint - 0x10000;
                        wchar_t high = static_cast<wchar_t>(0xD800 + (cp >> 10));
                        wchar_t low  = static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
                        processChar(high);
                        processChar(low);
                    } else {
                        processChar(static_cast<wchar_t>(m_utf8Codepoint));
                    }
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
            m_csiPrivate = false;
        } else if (ch == L'\r') {
            m_terminal.carriageReturn();
        } else if (ch == L'\n') {
            m_terminal.newline();
        } else if (ch == L'\t') {
            m_terminal.tab();
        } else if (ch == L'\b') {
            m_terminal.backspace();
        } else if (ch >= 32) {
            m_terminal.putChar(ch);
        }
        break;

    case STATE_ESC:
        if (ch == L'[') {
            m_state = STATE_CSI;
            m_sequence.clear();
            m_params.clear();
            m_currentParam = 0;
            m_hasParam = false;
            m_csiPrivate = false;
        } else if (ch == L's') {
            m_terminal.saveCursor();
            m_state = STATE_GROUND;
        } else if (ch == L']') {
            m_state = STATE_OSC;
            m_sequence.clear();
        } else if (ch == L'7') {
            m_terminal.saveCursor();
            m_state = STATE_GROUND;
        } else if (ch == L'8') {
            m_terminal.restoreCursor();
            m_state = STATE_GROUND;
        } else if (ch == L'D') {
            m_terminal.newline();
            m_state = STATE_GROUND;
        } else if (ch == L'M') {
            m_terminal.reverseIndex();
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
        } else if (ch == L'?') {
            m_csiPrivate = true;
        } else if (ch == L'>' || ch == L'=') {
            // Intermediate byte - ignore and continue
        } else if (ch >= L'@' && ch <= L'~') {
            if (m_hasParam || !m_params.empty()) {
                m_params.push_back(m_currentParam);
            }
            m_sequence.push_back(ch);
            executeCsi();
            m_state = STATE_GROUND;
            m_csiPrivate = false;
        } else {
            m_state = STATE_GROUND;
            m_csiPrivate = false;
        }
        break;

    case STATE_OSC:
        if (ch == L'\x07' || ch == L'\x1b') {
            if (m_sequence.size() > 0) {
                executeOsc();
            }
            m_state = STATE_GROUND;
        } else {
            m_sequence.push_back(ch);
        }
        break;

    case STATE_ESC_IGNORE:
        m_state = STATE_GROUND;
        break;
    }
}

void AnsiParser::writeResponse(const char* data, size_t len) {
    if (onWrite) onWrite(data, len);
}

void AnsiParser::executeCsi() {
    if (m_sequence.empty()) return;

    wchar_t finalChar = m_sequence.back();

    switch (finalChar) {
    case L'A': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.moveCursor(0, -n);
        break;
    }
    case L'B': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.moveCursor(0, n);
        break;
    }
    case L'C': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.moveCursor(n, 0);
        break;
    }
    case L'D': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.moveCursor(-n, 0);
        break;
    }
    case L'E': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.setCursorPos(0, m_terminal.getCursor().y + n);
        break;
    }
    case L'F': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.setCursorPos(0, m_terminal.getCursor().y - n);
        break;
    }
    case L'G': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.setCursorPos(n - 1, m_terminal.getCursor().y);
        break;
    }
    case L'H':
    case L'f': {
        int row = m_params.size() >= 1 ? m_params[0] : 1;
        int col = m_params.size() >= 2 ? m_params[1] : 1;
        m_terminal.setCursorPos(col - 1, row - 1);
        break;
    }
    case L'J': {
        int n = m_params.empty() ? 0 : m_params[0];
        switch (n) {
        case 0: m_terminal.eraseToEndOfScreen(); break;
        case 1: m_terminal.eraseToStartOfScreen(); break;
        case 2: m_terminal.eraseScreen(); break;
        case 3: m_terminal.eraseScreen(); break;
        }
        break;
    }
    case L'K': {
        int n = m_params.empty() ? 0 : m_params[0];
        switch (n) {
        case 0: m_terminal.eraseToEndOfLine(); break;
        case 1: m_terminal.eraseToStartOfLine(); break;
        case 2: m_terminal.eraseLine(); break;
        }
        break;
    }
    case L'L': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.insertLines(n);
        break;
    }
    case L'M': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.deleteLines(n);
        break;
    }
    case L'P': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.deleteChars(n);
        break;
    }
    case L'X': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.eraseChars(n);
        break;
    }
    case L'S': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.scrollUp(n);
        break;
    }
    case L'T': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.scrollDown(n);
        break;
    }
    case L'@': {
        int n = m_params.empty() ? 1 : m_params[0];
        m_terminal.insertChars(n);
        break;
    }
    case L'm': {
        executeSgr();
        break;
    }
    case L'r': {
        int top = m_params.size() >= 1 ? m_params[0] - 1 : 0;
        int bottom = m_params.size() >= 2 ? m_params[1] - 1 : m_terminal.getRows() - 1;
        m_terminal.setScrollRegion(top, bottom);
        break;
    }
    case L'h': {
        if (m_csiPrivate) {
            for (int p : m_params) {
                switch (p) {
                case 25: m_terminal.setCursorVisible(true); break;
                case 47:
                case 1047: m_terminal.switchToAlternateBuffer(); break;
                case 1048: m_terminal.saveCursor(); break;
                case 1049:
                    m_terminal.saveCursor();
                    m_terminal.switchToAlternateBuffer();
                    break;
                case 1000: m_terminal.setMode(1000, true); break;
                case 1002: m_terminal.setMode(1002, true); break;
                case 1006: m_terminal.setMode(1006, true); break;
                case 2004: m_terminal.setMode(2004, true); break;
                }
            }
        }
        break;
    }
    case L'l': {
        if (m_csiPrivate) {
            for (int p : m_params) {
                switch (p) {
                case 25: m_terminal.setCursorVisible(false); break;
                case 47:
                case 1047: m_terminal.switchToMainBuffer(); break;
                case 1048: m_terminal.restoreCursor(); break;
                case 1049:
                    m_terminal.switchToMainBuffer();
                    m_terminal.restoreCursor();
                    break;
                case 1000: m_terminal.setMode(1000, false); break;
                case 1002: m_terminal.setMode(1002, false); break;
                case 1006: m_terminal.setMode(1006, false); break;
                case 2004: m_terminal.setMode(2004, false); break;
                }
            }
        }
        break;
    }
    case L'n': {
        if (!m_params.empty() && m_params[0] == 6) {
            Cursor cur = m_terminal.getCursor();
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dR", cur.y + 1, cur.x + 1);
            writeResponse(buf, len);
        }
        break;
    }
    }
}

void AnsiParser::executeOsc() {
    if (m_sequence.empty()) return;

    std::wstring seq(m_sequence.begin(), m_sequence.end());

    size_t semi = seq.find(L';');
    if (semi == std::wstring::npos) return;

    int cmd = 0;
    for (size_t i = 0; i < semi; i++) {
        if (seq[i] >= L'0' && seq[i] <= L'9') {
            cmd = cmd * 10 + (seq[i] - L'0');
        }
    }

    if (cmd == 4) {
        size_t pos = semi + 1;
        while (pos < seq.size() && seq[pos] == L' ') pos++;

        int colorIdx = 0;
        while (pos < seq.size() && seq[pos] >= L'0' && seq[pos] <= L'9') {
            colorIdx = colorIdx * 10 + (seq[pos] - L'0');
            pos++;
        }
        if (pos < seq.size() && seq[pos] == L';') pos++;

        std::wstring spec;
        while (pos < seq.size()) {
            spec += seq[pos++];
        }

        if (colorIdx >= 0 && colorIdx < 16 && !spec.empty()) {
                if (spec[0] == L'#') {
                uint32_t color = 0;
                size_t start = 1;
                if (spec.size() >= 7) {
                    uint32_t r = 0, g = 0, b = 0;
                    for (int i = 0; i < 2; i++) {
                        wchar_t c = spec[start + i];
                        r = (r << 4) | (c >= L'A' ? (c - L'A' + 10) : (c - L'0'));
                    }
                    for (int i = 0; i < 2; i++) {
                        wchar_t c = spec[start + 2 + i];
                        g = (g << 4) | (c >= L'A' ? (c - L'A' + 10) : (c - L'0'));
                    }
                    for (int i = 0; i < 2; i++) {
                        wchar_t c = spec[start + 4 + i];
                        b = (b << 4) | (c >= L'A' ? (c - L'A' + 10) : (c - L'0'));
                    }
                    color = (r << 16) | (g << 8) | b;
                }
                if (spec.substr(0, 4) == L"rgb:") {
                    size_t start = 4;
                    uint32_t r = 0, g = 0, b = 0;
                    int shift = 8;
                    for (int i = 0; i < 2 && start + i < spec.size(); i++) {
                        wchar_t c = spec[start + i];
                        r = (r << 4) | (c >= L'A' ? (c - L'A' + 10) : (c - L'0'));
                    }
                    start += 2;
                    if (start < spec.size() && spec[start] == L'/') start++;
                    for (int i = 0; i < 2 && start + i < spec.size(); i++) {
                        wchar_t c = spec[start + i];
                        g = (g << 4) | (c >= L'A' ? (c - L'A' + 10) : (c - L'0'));
                    }
                    start += 2;
                    if (start < spec.size() && spec[start] == L'/') start++;
                    for (int i = 0; i < 2 && start + i < spec.size(); i++) {
                        wchar_t c = spec[start + i];
                        b = (b << 4) | (c >= L'A' ? (c - L'A' + 10) : (c - L'0'));
                    }
                    color = (r << 16) | (g << 8) | b;
                }
            }
        }
    } else if (cmd == 10 || cmd == 11) {
        // OSC 10/11: set fg/bg - parse but ignore for now
    } else if (cmd == 0 || cmd == 2) {
        // Window title - ignored
    }
}

static uint32_t ANSI_COLORS[] = {
    0x1A1B26,
    0xF7768E,
    0x9ECE6A,
    0xE0AF68,
    0x7AA2F7,
    0xBB9AF7,
    0x7DCFFF,
    0xA9B1D6,
    0x414868,
    0xF7768E,
    0x9ECE6A,
    0xE0AF68,
    0x7AA2F7,
    0xBB9AF7,
    0x7DCFFF,
    0xC0CAF5,
};

void AnsiParser::setAnsiColor(int index, uint32_t color) {
    if (index >= 0 && index < 16) {
        ANSI_COLORS[index] = color;
    }
}

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
            m_terminal.setDim(true);
        } else if (code == 3) {
            m_terminal.setItalic(true);
        } else if (code == 7) {
            m_terminal.setInverse(true);
        } else if (code == 22) {
            m_terminal.setBold(false);
            m_terminal.setDim(false);
        } else if (code == 23) {
            m_terminal.setItalic(false);
        } else if (code == 27) {
            m_terminal.setInverse(false);
        } else if (code >= 30 && code <= 37) {
            m_terminal.setFgColor(ANSI_COLORS[code - 30]);
        } else if (code == 38) {
            if (i + 1 < m_params.size()) {
                if (m_params[i + 1] == 5 && i + 2 < m_params.size()) {
                    int colorIdx = m_params[i + 2];
                    if (colorIdx < 16) {
                        m_terminal.setFgColor(ANSI_COLORS[colorIdx]);
                    } else if (colorIdx < 232) {
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
                        int gray = (colorIdx - 232) * 10 + 8;
                        m_terminal.setFgColor((gray << 16) | (gray << 8) | gray);
                    }
                    i += 2;
                } else if (m_params[i + 1] == 2 && i + 3 < m_params.size()) {
                    uint32_t r = m_params[i + 2];
                    uint32_t g = m_params[i + 3];
                    uint32_t b = m_params[i + 4];
                    m_terminal.setFgColor((r << 16) | (g << 8) | b);
                    i += 4;
                }
            }
        } else if (code == 39) {
            m_terminal.setFgColor(m_terminal.getDefaultFg());
        } else if (code >= 40 && code <= 47) {
            m_terminal.setBgColor(ANSI_COLORS[code - 40]);
        } else if (code == 48) {
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
            m_terminal.setBgColor(m_terminal.getDefaultBg());
        }

        i++;
    }
}
