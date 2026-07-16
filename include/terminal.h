#pragma once

#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>
#include <functional>
#include "unicode.h"

static const int MAX_COMBINED = 5;

struct Cell {
    wchar_t ch = L' ';
    uint32_t fg = 0xE0E0E0;
    uint32_t bg = 0x1A1B26;
    bool bold = false;
    bool italic = false;
    bool inverse = false;
    bool dim = false;
    uint8_t width = 1;
    wchar_t combined[MAX_COMBINED] = {};
    int combinedCount = 0;
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
    void scrollDown(int lines = 1);

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
    void setDim(bool d);
    void setItalic(bool i);
    void setInverse(bool inv);
    void resetAttributes();

    void setDefaultColors(uint32_t defaultFg, uint32_t defaultBg);
    uint32_t getDefaultFg() const { return m_defaultFg; }
    uint32_t getDefaultBg() const { return m_defaultBg; }

    void putChar(wchar_t ch);
    void backspace();
    void newline();
    void reverseIndex();
    void carriageReturn();
    void tab();

    void eraseToEndOfLine();
    void eraseToStartOfLine();
    void eraseLine();
    void eraseToEndOfScreen();
    void eraseToStartOfScreen();
    void eraseScreen();
    void eraseChars(int count);

    void insertLines(int count);
    void deleteLines(int count);
    void insertChars(int count);
    void deleteChars(int count);

    void setScrollRegion(int top, int bottom);

    void saveCursor();
    void restoreCursor();

    void switchToAlternateBuffer();
    void switchToMainBuffer();
    bool isAlternateBuffer() const { return m_altScreen; }

    // Terminal modes
    void setMode(int mode, bool enabled);
    bool getMode(int mode) const;

    // Mouse tracking
    bool isMouseTracking() const { return m_mode1000 || m_mode1002; }
    bool isMouseSGRMode() const { return m_mode1006; }
    bool isBracketedPaste() const { return m_mode2004; }

    // Scrollback
    int getScrollbackLines() const { return (int)m_scrollback.size(); }
    const std::vector<Cell>* getScrollbackLine(int index) const;
    void scrollBack(int lines = 1);
    void scrollForward(int lines = 1);
    void scrollToBottom();
    bool isScrolledBack() const { return m_scrollOffset > 0; }
    int getScrollOffset() const { return m_scrollOffset; }

    std::function<void(const char*, size_t)> onWrite;
    std::function<void(int, int)> onResize;
    std::function<void()> onBufferSwitch;

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
    bool m_dim = false;
    bool m_italic = false;
    bool m_inverse = false;

    uint32_t m_defaultFg = 0xE0E0E0;
    uint32_t m_defaultBg = 0x1A1B26;

    int m_scrollTop = 0;
    int m_scrollBottom = 0;

    Cursor m_savedCursor;
    int m_savedScrollTop = 0;
    int m_savedScrollBottom = 0;

    bool m_altScreen = false;
    std::vector<Cell> m_mainBuffer;
    Cursor m_mainCursor;
    int m_mainScrollTop = 0;
    int m_mainScrollBottom = 0;

    // Scrollback buffer
    static const int SCROLLBACK_LIMIT = 10000;
    std::vector<std::vector<Cell>> m_scrollback;
    int m_scrollOffset = 0; // 0 = at bottom, >0 = scrolled back

    void pushToScrollback(const std::vector<Cell>& line);

    // Terminal modes
    bool m_mode1000 = false; // Mouse tracking (X10)
    bool m_mode1002 = false; // Mouse tracking (button-event)
    bool m_mode1006 = false; // Mouse SGR encoding
    bool m_mode2004 = false; // Bracketed paste
};
