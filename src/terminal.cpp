#include "terminal.h"
#include <algorithm>
#include <cstring>

Terminal::Terminal(int cols, int rows)
    : m_cols(cols), m_rows(rows)
{
    m_scrollBottom = m_rows - 1;
    ensureCapacity();
    clear();
}

Terminal::~Terminal() = default;

bool Terminal::resize(int cols, int rows) {
    if (cols <= 0 || rows <= 0) return false;
    if (cols == m_cols && rows == m_rows) return false;

    std::vector<Cell> newBuffer(cols * rows);

    int copyCols = std::min(cols, m_cols);
    int copyRows = std::min(rows, m_rows);

    for (int y = 0; y < copyRows; y++) {
        for (int x = 0; x < copyCols; x++) {
            newBuffer[y * cols + x] = cellAt(x, y);
        }
    }

    m_cols = cols;
    m_rows = rows;
    m_buffer = std::move(newBuffer);

    m_cursor.x = std::min(m_cursor.x, m_cols - 1);
    m_cursor.y = std::min(m_cursor.y, m_rows - 1);
    m_scrollBottom = m_rows - 1;

    if (onResize) onResize(m_cols, m_rows);
    return true;
}

void Terminal::clear() {
    for (auto& cell : m_buffer) {
        cell = Cell{};
    }
    m_cursor.x = 0;
    m_cursor.y = 0;
}

void Terminal::scrollUp(int lines) {
    for (int i = 0; i < lines; i++) {
        for (int y = m_scrollTop; y < m_scrollBottom; y++) {
            for (int x = 0; x < m_cols; x++) {
                cellAt(x, y) = cellAt(x, y + 1);
            }
        }
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, m_scrollBottom) = Cell{};
        }
    }
}

void Terminal::write(const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char ch = data[i];
        if (ch == '\r') {
            carriageReturn();
        } else if (ch == '\n') {
            newline();
        } else if (ch == '\t') {
            tab();
        } else if (ch == '\b') {
            backspace();
        } else if (ch == '\x1b') {
            // Escape - handled by AnsiParser externally
        } else if (ch >= 32) {
            wchar_t wc = static_cast<wchar_t>(static_cast<unsigned char>(ch));
            putChar(wc);
        }
    }
}

void Terminal::write(const wchar_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        wchar_t ch = data[i];
        if (ch == L'\r') {
            carriageReturn();
        } else if (ch == L'\n') {
            newline();
        } else if (ch == L'\t') {
            tab();
        } else if (ch == L'\b') {
            backspace();
        } else if (ch >= 32) {
            putChar(ch);
        }
    }
}

void Terminal::putChar(wchar_t ch) {
    if (m_cursor.x >= m_cols) {
        m_cursor.x = 0;
        m_cursor.y++;
    }
    if (m_cursor.y > m_scrollBottom) {
        scrollUp(1);
        m_cursor.y = m_scrollBottom;
    }
    if (m_cursor.y >= m_rows) {
        m_cursor.y = m_rows - 1;
    }

    Cell& c = cellAt(m_cursor.x, m_cursor.y);
    c.ch = ch;
    c.fg = m_inverse ? m_currentBg : m_currentFg;
    c.bg = m_inverse ? m_currentFg : m_currentBg;
    c.bold = m_bold;

    m_cursor.x++;
}

void Terminal::backspace() {
    if (m_cursor.x > 0) {
        m_cursor.x--;
    }
}

void Terminal::newline() {
    m_cursor.y++;
    if (m_cursor.y > m_scrollBottom) {
        scrollUp(1);
        m_cursor.y = m_scrollBottom;
    }
}

void Terminal::carriageReturn() {
    m_cursor.x = 0;
}

void Terminal::tab() {
    int next = (m_cursor.x / 8 + 1) * 8;
    m_cursor.x = std::min(next, m_cols - 1);
}

void Terminal::eraseToEndOfLine() {
    for (int x = m_cursor.x; x < m_cols; x++) {
        cellAt(x, m_cursor.y) = Cell{};
    }
}

void Terminal::eraseToStartOfLine() {
    for (int x = 0; x <= m_cursor.x; x++) {
        cellAt(x, m_cursor.y) = Cell{};
    }
}

void Terminal::eraseLine() {
    for (int x = 0; x < m_cols; x++) {
        cellAt(x, m_cursor.y) = Cell{};
    }
}

void Terminal::eraseToEndOfScreen() {
    eraseToEndOfLine();
    for (int y = m_cursor.y + 1; y < m_rows; y++) {
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, y) = Cell{};
        }
    }
}

void Terminal::eraseToStartOfScreen() {
    eraseToStartOfLine();
    for (int y = 0; y < m_cursor.y; y++) {
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, y) = Cell{};
        }
    }
}

void Terminal::eraseScreen() {
    clear();
}

void Terminal::setScrollRegion(int top, int bottom) {
    m_scrollTop = std::max(0, std::min(top, m_rows - 1));
    m_scrollBottom = std::max(m_scrollTop, std::min(bottom, m_rows - 1));
    m_cursor.x = 0;
    m_cursor.y = 0;
}

void Terminal::setCursorPos(int x, int y) {
    m_cursor.x = std::clamp(x, 0, m_cols - 1);
    m_cursor.y = std::clamp(y, 0, m_rows - 1);
}

void Terminal::moveCursor(int dx, int dy) {
    m_cursor.x = std::clamp(m_cursor.x + dx, 0, m_cols - 1);
    m_cursor.y = std::clamp(m_cursor.y + dy, 0, m_rows - 1);
}

void Terminal::setCursorVisible(bool vis) {
    m_cursor.visible = vis;
}

void Terminal::setFgColor(uint32_t color) {
    m_currentFg = color;
}

void Terminal::setBgColor(uint32_t color) {
    m_currentBg = color;
}

void Terminal::setBold(bool b) {
    m_bold = b;
}

void Terminal::resetAttributes() {
    m_currentFg = 0xE0E0E0;
    m_currentBg = 0x1A1B26;
    m_bold = false;
    m_inverse = false;
}

Cell& Terminal::cellAt(int x, int y) {
    return m_buffer[y * m_cols + x];
}

const Cell& Terminal::cellAt(int x, int y) const {
    return m_buffer[y * m_cols + x];
}

void Terminal::ensureCapacity() {
    m_buffer.resize(m_cols * m_rows);
}
