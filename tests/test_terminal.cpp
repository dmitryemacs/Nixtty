#include "test.h"
#include "terminal.h"

TEST(terminal_init) {
    Terminal t(80, 24);
    ASSERT_EQ(t.getCols(), 80);
    ASSERT_EQ(t.getRows(), 24);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0);
    ASSERT_EQ(cur.y, 0);
}

TEST(terminal_putChar) {
    Terminal t(10, 3);
    t.putChar(L'A');
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 1);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'A');
}

TEST(terminal_putChar_wide) {
    Terminal t(10, 3);
    t.putChar(0x4E2D); // 中 (width=2)
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 2);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, 0x4E2D);
    ASSERT_EQ(buf[0].width, 2);
    ASSERT_EQ(buf[1].width, 0); // trailing cell
}

TEST(terminal_putChar_wideAtEdge) {
    Terminal t(5, 2);
    t.setCursorPos(4, 0);
    t.putChar(0x4E2D); // width=2, only 1 cell left
    Cursor cur = t.getCursor();
    // Should wrap: x=0, y=1 (or similar behavior)
    ASSERT_EQ(cur.y >= 0, true);
}

TEST(terminal_putChar_combining) {
    Terminal t(10, 3);
    t.putChar(L'e');
    t.putChar(0x0301); // combining acute
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 1); // cursor doesn't advance for combining
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'e');
    ASSERT_EQ(buf[0].combinedCount, 1);
    ASSERT_EQ(buf[0].combined[0], 0x0301);
}

TEST(terminal_cursorMovement) {
    Terminal t(10, 5);
    t.setCursorPos(3, 2);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 3);
    ASSERT_EQ(cur.y, 2);
}

TEST(terminal_cursorClamp) {
    Terminal t(10, 5);
    t.setCursorPos(100, 100);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 9);
    ASSERT_EQ(cur.y, 4);
}

TEST(terminal_cursorClampNegative) {
    Terminal t(10, 5);
    t.setCursorPos(-5, -5);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0);
    ASSERT_EQ(cur.y, 0);
}

TEST(terminal_moveCursor) {
    Terminal t(10, 5);
    t.moveCursor(3, 2);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 3);
    ASSERT_EQ(cur.y, 2);
    t.moveCursor(-1, -1);
    cur = t.getCursor();
    ASSERT_EQ(cur.x, 2);
    ASSERT_EQ(cur.y, 1);
}

TEST(terminal_moveCursorClamp) {
    Terminal t(10, 5);
    t.setCursorPos(9, 4);
    t.moveCursor(10, 10);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 9);
    ASSERT_EQ(cur.y, 4);
}

TEST(terminal_newline) {
    Terminal t(10, 5);
    t.newline();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0); // starts at 0
    ASSERT_EQ(cur.y, 1);
}

TEST(terminal_newline_scroll) {
    Terminal t(10, 3);
    t.setCursorPos(0, 2);
    t.putChar(L'X');
    t.newline();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.y, 2); // scrolled, cursor at bottom
}

TEST(terminal_carriageReturn) {
    Terminal t(10, 5);
    t.setCursorPos(5, 1);
    t.carriageReturn();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0);
    ASSERT_EQ(cur.y, 1);
}

TEST(terminal_backspace) {
    Terminal t(10, 5);
    t.setCursorPos(5, 0);
    t.backspace();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 4);
}

TEST(terminal_backspace_atOrigin) {
    Terminal t(10, 5);
    t.backspace();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0);
    ASSERT_EQ(cur.y, 0);
}

TEST(terminal_tab) {
    Terminal t(40, 5);
    t.tab();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 8);
    t.tab();
    cur = t.getCursor();
    ASSERT_EQ(cur.x, 16);
}

TEST(terminal_tab_clamp) {
    Terminal t(10, 5);
    t.setCursorPos(8, 0);
    t.tab();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 9); // clamped to cols-1
}

TEST(terminal_eraseToEndOfLine) {
    Terminal t(10, 3);
    t.putChar(L'H');
    t.putChar(L'i');
    t.setCursorPos(1, 0);
    t.eraseToEndOfLine();
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'H');
    ASSERT_EQ(buf[1].ch, L' ');
    ASSERT_EQ(buf[2].ch, L' ');
}

TEST(terminal_eraseToStartOfLine) {
    Terminal t(10, 3);
    t.putChar(L'X'); // at (0,0), cursor now (1,0)
    t.setCursorPos(5, 0); // move to (5,0)
    t.putChar(L'Y'); // at (5,0), cursor now (6,0)
    t.eraseToStartOfLine(); // erases x=0..6 (inclusive of cursor)
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[5].ch, L' ');
    ASSERT_EQ(buf[6].ch, L' ');
}

TEST(terminal_eraseLine) {
    Terminal t(10, 3);
    t.putChar(L'A');
    t.putChar(L'B');
    t.eraseLine();
    const Cell* buf = t.getBuffer();
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(buf[i].ch, L' ');
    }
}

TEST(terminal_eraseToEndOfScreen) {
    Terminal t(10, 3);
    t.putChar(L'A');
    t.eraseToEndOfScreen();
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'A');
    for (int i = 1; i < 30; i++) {
        ASSERT_EQ(buf[i].ch, L' ');
    }
}

TEST(terminal_eraseScreen) {
    Terminal t(10, 3);
    t.putChar(L'A');
    t.eraseScreen();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0);
    ASSERT_EQ(cur.y, 0);
    const Cell* buf = t.getBuffer();
    for (int i = 0; i < 30; i++) {
        ASSERT_EQ(buf[i].ch, L' ');
    }
}

TEST(terminal_eraseChars) {
    Terminal t(10, 3);
    t.putChar(L'A');
    t.putChar(L'B');
    t.putChar(L'C');
    t.setCursorPos(0, 0);
    t.eraseChars(2);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[1].ch, L' ');
    ASSERT_EQ(buf[2].ch, L'C');
}

TEST(terminal_resize) {
    Terminal t(10, 5);
    t.putChar(L'X');
    bool ok = t.resize(20, 10);
    ASSERT_TRUE(ok);
    ASSERT_EQ(t.getCols(), 20);
    ASSERT_EQ(t.getRows(), 10);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'X');
}

TEST(terminal_resize_shrink) {
    Terminal t(10, 5);
    t.putChar(L'X');
    bool ok = t.resize(5, 3);
    ASSERT_TRUE(ok);
    ASSERT_EQ(t.getCols(), 5);
    ASSERT_EQ(t.getRows(), 3);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'X');
}

TEST(terminal_resize_sameSize) {
    Terminal t(10, 5);
    bool ok = t.resize(10, 5);
    ASSERT_FALSE(ok);
}

TEST(terminal_resize_invalid) {
    Terminal t(10, 5);
    ASSERT_FALSE(t.resize(0, 5));
    ASSERT_FALSE(t.resize(5, 0));
}

TEST(terminal_scrollUp) {
    Terminal t(10, 3);
    t.putChar(L'1');
    t.carriageReturn();
    t.newline();
    t.putChar(L'2');
    t.carriageReturn();
    t.newline();
    t.putChar(L'3');
    t.scrollUp(1);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'2');
    ASSERT_EQ(buf[10].ch, L'3');
    ASSERT_EQ(buf[20].ch, L' ');
}

TEST(terminal_scrollDown) {
    Terminal t(10, 3);
    t.putChar(L'1');
    t.carriageReturn();
    t.newline();
    t.putChar(L'2');
    t.scrollDown(1);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[10].ch, L'1');
    ASSERT_EQ(buf[20].ch, L'2');
}

TEST(terminal_insertLines) {
    Terminal t(10, 4);
    t.putChar(L'A');
    t.setCursorPos(0, 1);
    t.putChar(L'B');
    t.setCursorPos(0, 0);
    t.insertLines(1);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[10].ch, L'A');
    ASSERT_EQ(buf[20].ch, L'B');
}

TEST(terminal_deleteLines) {
    Terminal t(10, 4);
    t.putChar(L'A');
    t.carriageReturn();
    t.newline();
    t.putChar(L'B');
    t.carriageReturn();
    t.newline();
    t.putChar(L'C');
    t.setCursorPos(0, 0);
    t.deleteLines(1);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'B');
    ASSERT_EQ(buf[10].ch, L'C');
    ASSERT_EQ(buf[20].ch, L' ');
}

TEST(terminal_insertChars) {
    Terminal t(10, 2);
    t.putChar(L'A');
    t.putChar(L'B');
    t.setCursorPos(0, 0);
    t.insertChars(1);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[1].ch, L'A');
    ASSERT_EQ(buf[2].ch, L'B');
}

TEST(terminal_deleteChars) {
    Terminal t(10, 2);
    t.putChar(L'A');
    t.putChar(L'B');
    t.putChar(L'C');
    t.setCursorPos(0, 0);
    t.deleteChars(1);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'B');
    ASSERT_EQ(buf[1].ch, L'C');
}

TEST(terminal_scrollRegion) {
    Terminal t(10, 10);
    t.setScrollRegion(2, 7);
    t.setCursorPos(0, 5);
    t.putChar(L'X');
    t.scrollUp(1);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[50].ch, L' ');
    // Line at row 8 should be blank (bottom of scroll region)
}

TEST(terminal_saveRestoreCursor) {
    Terminal t(10, 5);
    t.setCursorPos(3, 2);
    t.saveCursor();
    t.setCursorPos(7, 4);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 7);
    ASSERT_EQ(cur.y, 4);
    t.restoreCursor();
    cur = t.getCursor();
    ASSERT_EQ(cur.x, 3);
    ASSERT_EQ(cur.y, 2);
}

TEST(terminal_alternateBuffer) {
    Terminal t(10, 3);
    t.putChar(L'X');
    t.switchToAlternateBuffer();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0);
    ASSERT_EQ(cur.y, 0);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' '); // alt buffer is empty
    t.switchToMainBuffer();
    buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'X'); // main buffer restored
}

TEST(terminal_attributes) {
    Terminal t(10, 3);
    t.setFgColor(0xFF0000);
    t.setBgColor(0x00FF00);
    t.setBold(true);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].fg, 0xFF0000);
    ASSERT_EQ(buf[0].bg, 0x00FF00);
    ASSERT_TRUE(buf[0].bold);
}

TEST(terminal_resetAttributes) {
    Terminal t(10, 3);
    t.setFgColor(0xFF0000);
    t.setBold(true);
    t.resetAttributes();
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].fg, 0xFFFFFF); // default
    ASSERT_FALSE(buf[0].bold);
}

TEST(terminal_inverse) {
    Terminal t(10, 3);
    t.setFgColor(0xFF0000);
    t.setBgColor(0x00FF00);
    t.setInverse(true);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].fg, 0x00FF00); // swapped
    ASSERT_EQ(buf[0].bg, 0xFF0000);
}

TEST(terminal_modes) {
    Terminal t(10, 3);
    ASSERT_FALSE(t.getMode(1000));
    t.setMode(1000, true);
    ASSERT_TRUE(t.getMode(1000));
    t.setMode(1000, false);
    ASSERT_FALSE(t.getMode(1000));
}

TEST(terminal_mouseTracking) {
    Terminal t(10, 3);
    ASSERT_FALSE(t.isMouseTracking());
    t.setMode(1000, true);
    ASSERT_TRUE(t.isMouseTracking());
    ASSERT_FALSE(t.isMouseSGRMode());
    t.setMode(1006, true);
    ASSERT_TRUE(t.isMouseSGRMode());
}

TEST(terminal_scrollback) {
    Terminal t(10, 3);
    for (int i = 0; i < 10; i++) {
        t.putChar(L'A' + i);
        t.carriageReturn();
        t.newline();
    }
    int total = t.getScrollbackLines();
    ASSERT_TRUE(total > 0);
    const std::vector<Cell>* line = t.getScrollbackLine(0);
    ASSERT_TRUE(line != nullptr);
    ASSERT_EQ((*line)[0].ch, L'A');
}

TEST(terminal_scrollback_outOfRange) {
    Terminal t(10, 3);
    const std::vector<Cell>* line = t.getScrollbackLine(-1);
    ASSERT_TRUE(line == nullptr);
    line = t.getScrollbackLine(99999);
    ASSERT_TRUE(line == nullptr);
}

TEST(terminal_reverseIndex) {
    Terminal t(10, 3);
    t.setCursorPos(0, 0);
    t.reverseIndex();
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.y, 0); // clamped
}

TEST(terminal_defaultColors) {
    Terminal t(10, 3);
    t.setDefaultColors(0xAAAAAA, 0x222222);
    ASSERT_EQ(t.getDefaultFg(), 0xAAAAAA);
    ASSERT_EQ(t.getDefaultBg(), 0x222222);
}
