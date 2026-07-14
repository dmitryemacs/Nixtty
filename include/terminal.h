#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

struct Cell {
    wchar_t ch = L' ';
    uint32_t fg = 0xE0E0E0;
    uint32_t bg = 0x1A1B26;
    bool bold = false;
    bool inverse = false;
};

struct Cursor {
    int x = 0;
    int y = 0;
    bool visible = true;
    bool blink = true;
};

class Terminal {
public:
    Terminal(int cols, int rows);
    ~Terminal();

    bool resize(int cols, int rows);
    void clear();
    void scrollUp(int lines = 1);

    void write(const char* data, size_t len);
    void write(const wchar_t* data, size_t len);

    int getCols() const { return m_cols; }
    int getRows() const { return m_rows; }
    const Cell* getBuffer() const { return m_buffer.data(); }
    const Cursor& getCursor() const { return m_cursor; }

    void setCursorPos(int x, int y);
    void moveCursor(int dx, int dy);
    void setCursorVisible(bool vis);

    void setFgColor(uint32_t color);
    void setBgColor(uint32_t color);
    void setBold(bool b);
    void resetAttributes();

    void putChar(wchar_t ch);
    void backspace();
    void newline();
    void carriageReturn();
    void tab();

    // Erase operations
    void eraseToEndOfLine();
    void eraseToStartOfLine();
    void eraseLine();
    void eraseToEndOfScreen();
    void eraseToStartOfScreen();
    void eraseScreen();

    // Scroll region
    void setScrollRegion(int top, int bottom);

    // ANSI support
    std::function<void(int, int)> onResize;

private:
    Cell& cellAt(int x, int y);
    const Cell& cellAt(int x, int y) const;
    void ensureCapacity();

    int m_cols;
    int m_rows;
    std::vector<Cell> m_buffer;
    Cursor m_cursor;

    uint32_t m_currentFg = 0xE0E0E0;
    uint32_t m_currentBg = 0x1A1B26;
    bool m_bold = false;
    bool m_inverse = false;

    int m_scrollTop = 0;
    int m_scrollBottom = 0;

    // Saved cursor for ANSI ESC 7 / ESC 8
    Cursor m_savedCursor;
};
