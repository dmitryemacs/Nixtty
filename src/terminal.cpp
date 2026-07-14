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
    if (lines <= 0) return;
    lines = std::min(lines, m_scrollBottom - m_scrollTop + 1);

    // Push scrolled-off lines to scrollback (only when scrolling the full screen)
    if (m_scrollTop == 0 && !m_altScreen) {
        for (int i = 0; i < lines; i++) {
            int srcY = m_scrollTop + i;
            std::vector<Cell> line(m_cols);
            for (int x = 0; x < m_cols; x++) {
                line[x] = cellAt(x, srcY);
            }
            pushToScrollback(line);
        }
    }

    int rowsToMove = m_scrollBottom - m_scrollTop - lines + 1;
    if (rowsToMove > 0) {
        std::memmove(&cellAt(m_scrollTop, 0), &cellAt(m_scrollTop, lines),
                     rowsToMove * m_cols * sizeof(Cell));
    }
    for (int y = m_scrollBottom - lines + 1; y <= m_scrollBottom; y++) {
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, y) = Cell{};
        }
    }
}

void Terminal::scrollDown(int lines) {
    if (lines <= 0) return;
    lines = std::min(lines, m_scrollBottom - m_scrollTop + 1);
    int rowsToMove = m_scrollBottom - m_scrollTop - lines + 1;
    if (rowsToMove > 0) {
        std::memmove(&cellAt(m_scrollTop, lines), &cellAt(m_scrollTop, 0),
                     rowsToMove * m_cols * sizeof(Cell));
    }
    for (int y = m_scrollTop; y < m_scrollTop + lines; y++) {
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, y) = Cell{};
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
    c.inverse = m_inverse;

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

void Terminal::reverseIndex() {
    m_cursor.y--;
    if (m_cursor.y < m_scrollTop) {
        scrollDown(1);
        m_cursor.y = m_scrollTop;
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

void Terminal::eraseChars(int count) {
    count = std::min(count, m_cols - m_cursor.x);
    for (int x = m_cursor.x; x < m_cursor.x + count; x++) {
        cellAt(x, m_cursor.y) = Cell{};
    }
}

void Terminal::insertLines(int count) {
    if (m_cursor.y > m_scrollBottom) return;
    count = std::min(count, m_scrollBottom - m_cursor.y + 1);
    for (int y = m_scrollBottom; y >= m_cursor.y + count; y--) {
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, y) = cellAt(x, y - count);
        }
    }
    for (int y = m_cursor.y; y < m_cursor.y + count; y++) {
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, y) = Cell{};
        }
    }
}

void Terminal::deleteLines(int count) {
    if (m_cursor.y > m_scrollBottom) return;
    count = std::min(count, m_scrollBottom - m_cursor.y + 1);
    for (int y = m_cursor.y; y <= m_scrollBottom - count; y++) {
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, y) = cellAt(x, y + count);
        }
    }
    for (int y = m_scrollBottom - count + 1; y <= m_scrollBottom; y++) {
        for (int x = 0; x < m_cols; x++) {
            cellAt(x, y) = Cell{};
        }
    }
}

void Terminal::insertChars(int count) {
    count = std::min(count, m_cols - m_cursor.x);
    for (int x = m_cols - 1; x >= m_cursor.x + count; x--) {
        cellAt(x, m_cursor.y) = cellAt(x - count, m_cursor.y);
    }
    for (int x = m_cursor.x; x < m_cursor.x + count; x++) {
        cellAt(x, m_cursor.y) = Cell{};
    }
}

void Terminal::deleteChars(int count) {
    count = std::min(count, m_cols - m_cursor.x);
    for (int x = m_cursor.x; x <= m_cols - 1 - count; x++) {
        cellAt(x, m_cursor.y) = cellAt(x + count, m_cursor.y);
    }
    for (int x = m_cols - count; x < m_cols; x++) {
        cellAt(x, m_cursor.y) = Cell{};
    }
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

void Terminal::setInverse(bool inv) {
    m_inverse = inv;
}

void Terminal::resetAttributes() {
    m_currentFg = 0xE0E0E0;
    m_currentBg = 0x1A1B26;
    m_bold = false;
    m_inverse = false;
}

void Terminal::saveCursor() {
    m_savedCursor = m_cursor;
    m_savedScrollTop = m_scrollTop;
    m_savedScrollBottom = m_scrollBottom;
}

void Terminal::restoreCursor() {
    m_cursor = m_savedCursor;
    m_scrollTop = m_savedScrollTop;
    m_scrollBottom = m_savedScrollBottom;
}

void Terminal::switchToAlternateBuffer() {
    if (m_altScreen) return;
    m_mainBuffer = m_buffer;
    m_mainCursor = m_cursor;
    m_mainScrollTop = m_scrollTop;
    m_mainScrollBottom = m_scrollBottom;
    m_altScreen = true;
    m_buffer.assign(m_cols * m_rows, Cell{});
    m_cursor = Cursor{};
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
}

void Terminal::switchToMainBuffer() {
    if (!m_altScreen) return;
    m_buffer = std::move(m_mainBuffer);
    m_cursor = m_mainCursor;
    m_scrollTop = m_mainScrollTop;
    m_scrollBottom = m_mainScrollBottom;
    m_altScreen = false;
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

void Terminal::pushToScrollback(const std::vector<Cell>& line) {
    m_scrollback.push_back(line);
    if ((int)m_scrollback.size() > SCROLLBACK_LIMIT) {
        m_scrollback.erase(m_scrollback.begin());
    }
}

void Terminal::scrollBack(int lines) {
    lines = std::min(lines, (int)m_scrollback.size() - m_scrollOffset);
    if (lines <= 0) return;
    m_scrollOffset += lines;
}

void Terminal::scrollForward(int lines) {
    lines = std::min(lines, m_scrollOffset);
    if (lines <= 0) return;
    m_scrollOffset -= lines;
}

void Terminal::scrollToBottom() {
    m_scrollOffset = 0;
}

const std::vector<Cell>* Terminal::getScrollbackLine(int index) const {
    if (index < 0 || index >= (int)m_scrollback.size()) return nullptr;
    return &m_scrollback[index];
}
