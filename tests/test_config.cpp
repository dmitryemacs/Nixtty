#include "test.h"
#include "config.h"
#include <cstdio>
#include <fstream>

static void writeFile(const char* path, const char* content) {
    std::ofstream f(path);
    f << content;
}

TEST(config_parseColor_hex6) {
    // We can't call parseColor directly (it's static), but we can test
    // through loadFromFile which uses it internally
    Config c;
    writeFile("_test_color.toml", "background = \"#FF00AA\"\n");
    ASSERT_TRUE(c.loadFromFile("_test_color.toml"));
    ASSERT_EQ(c.background, (uint32_t)0xFF00AA);
    remove("_test_color.toml");
}

TEST(config_parseColor_hex3) {
    Config c;
    writeFile("_test_color.toml", "background = \"#F0A\"\n");
    ASSERT_TRUE(c.loadFromFile("_test_color.toml"));
    ASSERT_EQ(c.background, (uint32_t)0xFF00AA);
    remove("_test_color.toml");
}

TEST(config_parseColor_withoutHash) {
    Config c;
    writeFile("_test_color.toml", "foreground = \"C0CAF5\"\n");
    ASSERT_TRUE(c.loadFromFile("_test_color.toml"));
    ASSERT_EQ(c.foreground, (uint32_t)0xC0CAF5);
    remove("_test_color.toml");
}

TEST(config_fontSize) {
    Config c;
    writeFile("_test_size.toml", "size = 20\n");
    ASSERT_TRUE(c.loadFromFile("_test_size.toml"));
    ASSERT_EQ(c.fontSize, 20);
    remove("_test_size.toml");
}

TEST(config_fontFamily) {
    Config c;
    writeFile("_test_family.toml", "family = \"Consolas\"\n");
    ASSERT_TRUE(c.loadFromFile("_test_family.toml"));
    ASSERT_STREQ(c.fontFamily.c_str(), "Consolas");
    remove("_test_family.toml");
}

TEST(config_opacity) {
    Config c;
    writeFile("_test_opacity.toml", "opacity = 0.85\n");
    ASSERT_TRUE(c.loadFromFile("_test_opacity.toml"));
    ASSERT_FLOAT_EQ(c.opacity, 0.85f);
    remove("_test_opacity.toml");
}

TEST(config_ansiColors) {
    Config c;
    writeFile("_test_ansi.toml",
        "color0 = \"#1A1B26\"\n"
        "color1 = \"#F7768E\"\n"
        "color15 = \"#C0CAF5\"\n");
    ASSERT_TRUE(c.loadFromFile("_test_ansi.toml"));
    ASSERT_EQ(c.ansiColors[0], (uint32_t)0x1A1B26);
    ASSERT_EQ(c.ansiColors[1], (uint32_t)0xF7768E);
    ASSERT_EQ(c.ansiColors[15], (uint32_t)0xC0CAF5);
    remove("_test_ansi.toml");
}

TEST(config_comments) {
    Config c;
    writeFile("_test_comments.toml",
        "# this is a comment\n"
        "size = 14\n"
        "# another comment\n");
    ASSERT_TRUE(c.loadFromFile("_test_comments.toml"));
    ASSERT_EQ(c.fontSize, 14);
    remove("_test_comments.toml");
}

TEST(config_sections) {
    Config c;
    writeFile("_test_sections.toml",
        "[font]\n"
        "size = 22\n");
    ASSERT_TRUE(c.loadFromFile("_test_sections.toml"));
    ASSERT_EQ(c.fontSize, 22); // [font] is a section header, ignored; size=22 parsed
    remove("_test_sections.toml");
}

TEST(config_fileNotFound) {
    Config c;
    ASSERT_FALSE(c.loadFromFile("nonexistent_file_12345.toml"));
}

TEST(config_emptyFile) {
    Config c;
    writeFile("_test_empty.toml", "");
    ASSERT_TRUE(c.loadFromFile("_test_empty.toml"));
    remove("_test_empty.toml");
}

TEST(config_full) {
    Config c;
    writeFile("_test_full.toml",
        "family = \"Hack Nerd Font\"\n"
        "size = 18\n"
        "background = \"#1A1B26\"\n"
        "foreground = \"#C0CAF5\"\n"
        "cursor = \"#FFFFFF\"\n"
        "selection = \"#4D6299\"\n"
        "opacity = 0.95\n"
        "color0 = \"#000000\"\n"
        "color7 = \"#FFFFFF\"\n");
    ASSERT_TRUE(c.loadFromFile("_test_full.toml"));
    ASSERT_STREQ(c.fontFamily.c_str(), "Hack Nerd Font");
    ASSERT_EQ(c.fontSize, 18);
    ASSERT_EQ(c.background, (uint32_t)0x1A1B26);
    ASSERT_EQ(c.foreground, (uint32_t)0xC0CAF5);
    ASSERT_EQ(c.cursor, (uint32_t)0xFFFFFF);
    ASSERT_EQ(c.selection, (uint32_t)0x4D6299);
    ASSERT_FLOAT_EQ(c.opacity, 0.95f);
    ASSERT_EQ(c.ansiColors[0], (uint32_t)0x000000);
    ASSERT_EQ(c.ansiColors[7], (uint32_t)0xFFFFFF);
    remove("_test_full.toml");
}

TEST(config_quotedValues) {
    Config c;
    writeFile("_test_quotes.toml", "family = \"Courier New\"\n");
    ASSERT_TRUE(c.loadFromFile("_test_quotes.toml"));
    ASSERT_STREQ(c.fontFamily.c_str(), "Courier New");
    remove("_test_quotes.toml");
}

TEST(config_defaults) {
    Config c;
    // Without loading any file, defaults should be set
    ASSERT_EQ(c.fontSize, 16);
    ASSERT_EQ(c.background, (uint32_t)0x1E1E1E);
    ASSERT_EQ(c.foreground, (uint32_t)0xFFFFFF);
    ASSERT_FLOAT_EQ(c.opacity, 1.0f);
    ASSERT_EQ(c.cols, 100);
    ASSERT_EQ(c.rows, 30);
    ASSERT_EQ(c.scrollback, 10000);
    ASSERT_EQ(c.cursorBlink, true);
    ASSERT_EQ(c.cursorBlinkMs, 500);
}

TEST(config_terminal_cols) {
    Config c;
    writeFile("_test_cols.toml", "cols = 120\n");
    ASSERT_TRUE(c.loadFromFile("_test_cols.toml"));
    ASSERT_EQ(c.cols, 120);
    remove("_test_cols.toml");
}

TEST(config_terminal_rows) {
    Config c;
    writeFile("_test_rows.toml", "rows = 40\n");
    ASSERT_TRUE(c.loadFromFile("_test_rows.toml"));
    ASSERT_EQ(c.rows, 40);
    remove("_test_rows.toml");
}

TEST(config_terminal_scrollback) {
    Config c;
    writeFile("_test_sb.toml", "scrollback = 50000\n");
    ASSERT_TRUE(c.loadFromFile("_test_sb.toml"));
    ASSERT_EQ(c.scrollback, 50000);
    remove("_test_sb.toml");
}

TEST(config_terminal_cursorBlink) {
    Config c;
    writeFile("_test_blink.toml", "cursor_blink = false\n");
    ASSERT_TRUE(c.loadFromFile("_test_blink.toml"));
    ASSERT_EQ(c.cursorBlink, false);
    remove("_test_blink.toml");
}

TEST(config_terminal_cursorBlinkMs) {
    Config c;
    writeFile("_test_blinkms.toml", "cursor_blink_ms = 300\n");
    ASSERT_TRUE(c.loadFromFile("_test_blinkms.toml"));
    ASSERT_EQ(c.cursorBlinkMs, 300);
    remove("_test_blinkms.toml");
}
