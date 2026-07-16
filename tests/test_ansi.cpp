#include "test.h"
#include "ansi.h"
#include "terminal.h"

TEST(ansi_parse_ascii) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("ABC", 3);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'A');
    ASSERT_EQ(buf[1].ch, L'B');
    ASSERT_EQ(buf[2].ch, L'C');
}

TEST(ansi_parse_newline) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("A\nB", 3);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'A');
    // \n only moves cursor down, doesn't reset x; A at (0,0), \n -> (0,1), B at (1,1)
    ASSERT_EQ(buf[11].ch, L'B');
}

TEST(ansi_parse_carriageReturn) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("ABC\rX", 5);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'X');
}

TEST(ansi_parse_tab) {
    Terminal t(40, 3);
    AnsiParser p(t);
    p.parse("A\tB", 3);
    Cursor cur = t.getCursor();
    // A at 0, tab to 8, B at 8, cursor at 9
    ASSERT_EQ(cur.x, 9);
}

TEST(ansi_parse_backspace) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("AB\bC", 4);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'A');
    ASSERT_EQ(buf[1].ch, L'C');
}

TEST(ansi_csi_cursorUp) {
    Terminal t(10, 5);
    t.setCursorPos(3, 3);
    AnsiParser p(t);
    p.parse("\x1b[2A", 4);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 3);
    ASSERT_EQ(cur.y, 1);
}

TEST(ansi_csi_cursorDown) {
    Terminal t(10, 5);
    AnsiParser p(t);
    p.parse("\x1b[3B", 4);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.y, 3);
}

TEST(ansi_csi_cursorForward) {
    Terminal t(10, 5);
    AnsiParser p(t);
    p.parse("\x1b[5C", 4);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 5);
}

TEST(ansi_csi_cursorBack) {
    Terminal t(10, 5);
    t.setCursorPos(5, 0);
    AnsiParser p(t);
    p.parse("\x1b[3D", 4);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 2);
}

TEST(ansi_csi_cursorPosition) {
    Terminal t(10, 5);
    AnsiParser p(t);
    p.parse("\x1b[3;5H", 6);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.y, 2); // row 3 -> y=2
    ASSERT_EQ(cur.x, 4); // col 5 -> x=4
}

TEST(ansi_csi_cursorPositionDefault) {
    Terminal t(10, 5);
    t.setCursorPos(5, 3);
    AnsiParser p(t);
    p.parse("\x1b[H", 3);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0);
    ASSERT_EQ(cur.y, 0);
}

TEST(ansi_csi_eraseDisplay) {
    Terminal t(10, 3);
    t.putChar(L'X');
    AnsiParser p(t);
    p.parse("\x1b[2J", 4);
    Cursor cur = t.getCursor();
    ASSERT_EQ(cur.x, 0);
    ASSERT_EQ(cur.y, 0);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
}

TEST(ansi_csi_eraseLine) {
    Terminal t(10, 3);
    t.putChar(L'X');
    t.putChar(L'Y');
    AnsiParser p(t);
    p.parse("\x1b[2K", 4);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[1].ch, L' ');
}

TEST(ansi_csi_eraseToEndOfLine) {
    Terminal t(10, 3);
    t.putChar(L'X');
    t.putChar(L'Y');
    // cursor is now at (2,0); erase from (2,0) to end of line
    AnsiParser p(t);
    p.parse("\x1b[K", 3);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'X');
    ASSERT_EQ(buf[1].ch, L'Y');
    ASSERT_EQ(buf[2].ch, L' ');
}

TEST(ansi_csi_insertLines) {
    Terminal t(10, 4);
    t.putChar(L'A');
    t.setCursorPos(0, 1);
    t.putChar(L'B');
    t.setCursorPos(0, 0);
    AnsiParser p(t);
    p.parse("\x1b[1L", 4);
    const Cell* buf = t.getBuffer();
    // row0 cleared, A pushed from row0->row1, B pushed from row1->row2
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[10].ch, L'A');
    ASSERT_EQ(buf[20].ch, L'B');
}

TEST(ansi_csi_deleteLines) {
    Terminal t(10, 4);
    t.putChar(L'A');
    t.carriageReturn();
    t.newline();
    t.putChar(L'B');
    t.setCursorPos(0, 0);
    AnsiParser p(t);
    p.parse("\x1b[1M", 4);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'B');
}

TEST(ansi_csi_insertChars) {
    Terminal t(10, 2);
    t.putChar(L'A');
    t.putChar(L'B');
    t.setCursorPos(0, 0);
    AnsiParser p(t);
    p.parse("\x1b[1@", 4);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[1].ch, L'A');
}

TEST(ansi_csi_deleteChars) {
    Terminal t(10, 2);
    t.putChar(L'A');
    t.putChar(L'B');
    t.setCursorPos(0, 0);
    AnsiParser p(t);
    p.parse("\x1b[1P", 4);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'B');
}

TEST(ansi_csi_scrollUp) {
    Terminal t(10, 4);
    t.putChar(L'1');
    t.carriageReturn();
    t.newline();
    t.putChar(L'2');
    t.carriageReturn();
    t.newline();
    t.putChar(L'3');
    t.carriageReturn();
    t.newline();
    t.putChar(L'4');
    t.setCursorPos(0, 0);
    AnsiParser p(t);
    p.parse("\x1b[1S", 4);
    const Cell* buf = t.getBuffer();
    // scrollUp pushes row 0 to scrollback, rows shift up
    // row0='2', row1='3', row2='4', row3=empty
    ASSERT_EQ(buf[0].ch, L'2');
    ASSERT_EQ(buf[10].ch, L'3');
    ASSERT_EQ(buf[20].ch, L'4');
    ASSERT_EQ(buf[30].ch, L' ');
}

TEST(ansi_csi_scrollDown) {
    Terminal t(10, 4);
    t.putChar(L'1');
    t.carriageReturn();
    t.newline();
    t.putChar(L'2');
    t.setCursorPos(0, 0);
    AnsiParser p(t);
    p.parse("\x1b[1T", 4);
    const Cell* buf = t.getBuffer();
    // scrollDown shifts rows down, row0 cleared
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[10].ch, L'1');
    ASSERT_EQ(buf[20].ch, L'2');
}

TEST(ansi_csi_eraseChars) {
    Terminal t(10, 2);
    t.putChar(L'A');
    t.putChar(L'B');
    t.putChar(L'C');
    t.setCursorPos(0, 0);
    AnsiParser p(t);
    p.parse("\x1b[2X", 4);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    ASSERT_EQ(buf[1].ch, L' ');
    ASSERT_EQ(buf[2].ch, L'C');
}

TEST(ansi_sgr_bold) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[1m", 4);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_TRUE(buf[0].bold);
    p.parse("\x1b[22m", 5);
    t.putChar(L'Y');
    ASSERT_FALSE(buf[1].bold);
}

TEST(ansi_sgr_dim) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[2m", 4);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_TRUE(buf[0].dim);
}

TEST(ansi_sgr_italic) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[3m", 4);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_TRUE(buf[0].italic);
}

TEST(ansi_sgr_inverse) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[7m", 4);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_TRUE(buf[0].inverse);
}

TEST(ansi_sgr_reset) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[1m\x1b[7m", 8);
    p.parse("\x1b[0m", 4);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_FALSE(buf[0].bold);
    ASSERT_FALSE(buf[0].inverse);
}

TEST(ansi_sgr_fgColor) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[31m", 5); // red
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].fg, 0xF7768E); // ANSI red from default palette
}

TEST(ansi_sgr_bgColor) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[44m", 5); // blue bg
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].bg, 0x7AA2F7); // ANSI blue
}

TEST(ansi_sgr_256color_fg) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[38;5;196m", 11); // 256-color: color 196
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    // 196 is in the 6x6x6 cube: idx=180, r=6,g=0,b=0
    // r=6 -> 6*40+55=295 -> 0x127... but let me compute
    // idx = 196-16 = 180; b=180%6=0; g=(180/6)%6=0; r=180/36=5
    // r!=0: 5*40+55=255; g=0: 0x55=85; b=0: 0x55=85
    // color = (255<<16)|(85<<8)|85 = 0xFF5555
    ASSERT_EQ(buf[0].fg, (uint32_t)0xFF5555);
}

TEST(ansi_sgr_trueColor_fg) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[38;2;100;200;50m", 18);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].fg, (uint32_t)0x64C832);
}

TEST(ansi_sgr_defaultFg) {
    Terminal t(10, 3);
    t.setDefaultColors(0xAAAAAA, 0x111111);
    AnsiParser p(t);
    p.parse("\x1b[31m", 5);
    p.parse("\x1b[39m", 5);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].fg, 0xAAAAAA);
}

TEST(ansi_sgr_defaultBg) {
    Terminal t(10, 3);
    t.setDefaultColors(0xAAAAAA, 0x111111);
    AnsiParser p(t);
    p.parse("\x1b[41m", 5);
    p.parse("\x1b[49m", 5);
    t.putChar(L'X');
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].bg, 0x111111);
}

TEST(ansi_utf8_twoByte) {
    Terminal t(10, 3);
    AnsiParser p(t);
    // é = U+00E9 = UTF-8: 0xC3 0xA9
    const char data[] = {(char)0xC3, (char)0xA9};
    p.parse(data, 2);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, 0x00E9);
}

TEST(ansi_utf8_threeByte) {
    Terminal t(10, 3);
    AnsiParser p(t);
    // 中 = U+4E2D = UTF-8: 0xE4 0xB8 0xAD
    const char data[] = {(char)0xE4, (char)0xB8, (char)0xAD};
    p.parse(data, 3);
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, 0x4E2D);
}

TEST(ansi_utf8_fourByte) {
    Terminal t(10, 5);
    AnsiParser p(t);
    // 😀 = U+1F600 = UTF-8: 0xF0 0x9F 0x98 0x80
    const char data[] = {(char)0xF0, (char)0x9F, (char)0x98, (char)0x80};
    p.parse(data, 4);
    Cursor cur = t.getCursor();
    // Should produce surrogate pair, cursor advances
    ASSERT_TRUE(cur.x > 0);
}

TEST(ansi_privateMode_enable) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[?25l", 6); // hide cursor
    ASSERT_FALSE(t.getCursor().visible);
    p.parse("\x1b[?25h", 6); // show cursor
    ASSERT_TRUE(t.getCursor().visible);
}

TEST(ansi_privateMode_alternateBuffer) {
    Terminal t(10, 3);
    t.putChar(L'X');
    AnsiParser p(t);
    p.parse("\x1b[?1049h", 8); // switch to alt
    ASSERT_TRUE(t.isAlternateBuffer());
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L' ');
    p.parse("\x1b[?1049l", 8); // switch to main
    ASSERT_FALSE(t.isAlternateBuffer());
    buf = t.getBuffer();
    ASSERT_EQ(buf[0].ch, L'X');
}

TEST(ansi_privateMode_mouseTracking) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[?1000h", 8);
    ASSERT_TRUE(t.isMouseTracking());
    p.parse("\x1b[?1000l", 8);
    ASSERT_FALSE(t.isMouseTracking());
}

TEST(ansi_privateMode_bracketedPaste) {
    Terminal t(10, 3);
    AnsiParser p(t);
    p.parse("\x1b[?2004h", 8);
    ASSERT_TRUE(t.isBracketedPaste());
    p.parse("\x1b[?2004l", 8);
    ASSERT_FALSE(t.isBracketedPaste());
}

TEST(ansi_scrollRegion_set) {
    Terminal t(10, 10);
    AnsiParser p(t);
    p.parse("\x1b[3;8r", 6);
    t.setCursorPos(0, 5);
    t.putChar(L'X');
    t.scrollUp(1);
    // After scrollUp, line at row 5 (inside scroll region) is cleared
    const Cell* buf = t.getBuffer();
    ASSERT_EQ(buf[50].ch, L' '); // row 5 cleared
}

TEST(ansi_cursorPosition_report) {
    Terminal t(10, 5);
    t.setCursorPos(4, 2);
    AnsiParser p(t);
    std::string response;
    p.onWrite = [&](const char* data, size_t len) {
        response.assign(data, len);
    };
    p.parse("\x1b[6n", 4); // DSR
    ASSERT_STREQ(response.c_str(), "\x1b[3;5R");
}
