#include "test.h"
#include "unicode.h"

// --- ucsWidth ---

TEST(ucsWidth_null) {
    ASSERT_EQ(ucsWidth(0), 0);
}

TEST(ucsWidth_ascii) {
    ASSERT_EQ(ucsWidth('A'), 1);
    ASSERT_EQ(ucsWidth('z'), 1);
    ASSERT_EQ(ucsWidth('0'), 1);
    ASSERT_EQ(ucsWidth(' '), 1);
    ASSERT_EQ(ucsWidth('@'), 1);
    ASSERT_EQ(ucsWidth('~'), 1);
}

TEST(ucsWidth_combining) {
    ASSERT_EQ(ucsWidth(0x0300), 0); // Combining Grave Accent
    ASSERT_EQ(ucsWidth(0x0301), 0); // Combining Acute Accent
    ASSERT_EQ(ucsWidth(0x0308), 0); // Combining Diaeresis
    ASSERT_EQ(ucsWidth(0x0327), 0); // Combining Cedilla
}

TEST(ucsWidth_variationSelector) {
    ASSERT_EQ(ucsWidth(0xFE00), 0); // VS1
    ASSERT_EQ(ucsWidth(0xFE0F), 0); // VS16
}

TEST(ucsWidth_zeroWidthSpace) {
    ASSERT_EQ(ucsWidth(0x200B), 0);
    ASSERT_EQ(ucsWidth(0x200C), 0);
    ASSERT_EQ(ucsWidth(0x200D), 0); // ZWJ
}

TEST(ucsWidth_cjk) {
    ASSERT_EQ(ucsWidth(0x4E00), 2); // CJK Unified Ideograph
    ASSERT_EQ(ucsWidth(0x4E2D), 2); // 中
    ASSERT_EQ(ucsWidth(0x65E5), 2); // 日
    ASSERT_EQ(ucsWidth(0x95E8), 2); // 门
}

TEST(ucsWidth_hiragana) {
    ASSERT_EQ(ucsWidth(0x3041), 2); // あ
    ASSERT_EQ(ucsWidth(0x3042), 2);
    ASSERT_EQ(ucsWidth(0x3096), 2);
}

TEST(ucsWidth_katakana) {
    ASSERT_EQ(ucsWidth(0x30A1), 2); // ァ
    ASSERT_EQ(ucsWidth(0x30A2), 2); // ア
    ASSERT_EQ(ucsWidth(0x30FF), 2);
}

TEST(ucsWidth_fullwidth) {
    ASSERT_EQ(ucsWidth(0xFF01), 2); // ！
    ASSERT_EQ(ucsWidth(0xFF21), 2); // Ａ
    ASSERT_EQ(ucsWidth(0xFF60), 2);
}

TEST(ucsWidth_hangul) {
    ASSERT_EQ(ucsWidth(0xAC00), 2); // 가
    ASSERT_EQ(ucsWidth(0xD7AF), 2);
}

TEST(ucsWidth_latinExtended) {
    ASSERT_EQ(ucsWidth(0x00C0), 1); // À
    ASSERT_EQ(ucsWidth(0x00FF), 1); // ÿ
    ASSERT_EQ(ucsWidth(0x0100), 1); // Ā
}

// --- isCombining ---

TEST(isCombining_accent) {
    ASSERT_TRUE(isCombining(0x0300));
    ASSERT_TRUE(isCombining(0x0301));
    ASSERT_TRUE(isCombining(0x0308));
}

TEST(isCombining_variationSelector) {
    ASSERT_TRUE(isCombining(0xFE00));
    ASSERT_TRUE(isCombining(0xFE0F));
}

TEST(isCombining_normalChars) {
    ASSERT_FALSE(isCombining('A'));
    ASSERT_FALSE(isCombining(0x4E00));
    ASSERT_TRUE(isCombining(0x200B)); // Zero Width Space is zero-width
}

// --- isBoxDrawing ---

TEST(isBoxDrawing_basic) {
    ASSERT_TRUE(isBoxDrawing(0x2500)); // ─
    ASSERT_TRUE(isBoxDrawing(0x2502)); // │
    ASSERT_TRUE(isBoxDrawing(0x250C)); // ┌
    ASSERT_TRUE(isBoxDrawing(0x2510)); // ┐
    ASSERT_TRUE(isBoxDrawing(0x2514)); // └
    ASSERT_TRUE(isBoxDrawing(0x2518)); // ┘
    ASSERT_TRUE(isBoxDrawing(0x251C)); // ├
    ASSERT_TRUE(isBoxDrawing(0x2524)); // ┤
    ASSERT_TRUE(isBoxDrawing(0x252C)); // ┬
    ASSERT_TRUE(isBoxDrawing(0x2534)); // ┴
    ASSERT_TRUE(isBoxDrawing(0x253C)); // ┼
    ASSERT_TRUE(isBoxDrawing(0x257F)); // ┿
}

TEST(isBoxDrawing_outOfRange) {
    ASSERT_FALSE(isBoxDrawing(0x24FF));
    ASSERT_FALSE(isBoxDrawing(0x2580));
    ASSERT_FALSE(isBoxDrawing('A'));
    ASSERT_FALSE(isBoxDrawing(0x4E00));
}

// --- isEmoji ---

TEST(isEmoji_emoticons) {
    ASSERT_TRUE(isEmoji(0x1F600)); // 😀
    ASSERT_TRUE(isEmoji(0x1F64F)); // 🙏
}

TEST(isEmoji_miscSymbols) {
    ASSERT_TRUE(isEmoji(0x2600)); // ☀
    ASSERT_TRUE(isEmoji(0x26FF)); // ⛿
}

TEST(isEmoji_dingbats) {
    ASSERT_TRUE(isEmoji(0x2700)); // ✀
    ASSERT_TRUE(isEmoji(0x27BF)); // ➿
}

TEST(isEmoji_transport) {
    ASSERT_TRUE(isEmoji(0x1F680)); // 🚀
    ASSERT_TRUE(isEmoji(0x1F6FF));
}

TEST(isEmoji_regionalIndicator) {
    ASSERT_TRUE(isEmoji(0x1F1E0)); // 🇦
    ASSERT_TRUE(isEmoji(0x1F1FF)); // 🇿
}

TEST(isEmoji_notEmoji) {
    ASSERT_FALSE(isEmoji('A'));
    ASSERT_FALSE(isEmoji(0x100)); // Latin Extended
    ASSERT_FALSE(isEmoji(0x200)); // Spacing Modifier Letters
}

// --- shouldContinueGrapheme ---

TEST(shouldContinueGrapheme_combining) {
    ASSERT_TRUE(shouldContinueGrapheme('e', 0x0301)); // e + combining acute
    ASSERT_TRUE(shouldContinueGrapheme(0x4E00, 0x0308)); // CJK + combining diaeresis
}

TEST(shouldContinueGrapheme_variationSelector) {
    ASSERT_TRUE(shouldContinueGrapheme(0x1F600, 0xFE0F)); // emoji + VS16
}

TEST(shouldContinueGrapheme_zwj) {
    ASSERT_TRUE(shouldContinueGrapheme(0x200D, 0x1F600)); // ZWJ + emoji
    ASSERT_TRUE(shouldContinueGrapheme('A', 0x200D)); // char + ZWJ
}

TEST(shouldContinueGrapheme_normal) {
    ASSERT_FALSE(shouldContinueGrapheme('A', 'B'));
    ASSERT_FALSE(shouldContinueGrapheme(0x4E00, 0x4E2D));
}
