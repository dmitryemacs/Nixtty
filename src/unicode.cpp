#include "unicode.h"

// Wide character ranges (width = 2)
// Based on Unicode 15.0 East Asian Width property
static bool isWide(uint32_t cp) {
    // CJK Radicals Supplement .. Ideographic Description Characters
    if (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) return true;
    // CJK Unified Ideographs Extension A
    if (cp >= 0x3400 && cp <= 0x4DBF) return true;
    // CJK Unified Ideographs
    if (cp >= 0x4E00 && cp <= 0x9FFF) return true;
    // CJK Compatibility Ideographs
    if (cp >= 0xF900 && cp <= 0xFAFF) return true;
    // CJK Compatibility Forms
    if (cp >= 0xFE30 && cp <= 0xFE4F) return true;
    // Fullwidth Forms
    if (cp >= 0xFF01 && cp <= 0xFF60) return true;
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return true;
    // CJK Unified Ideographs Extension B (surrogate pairs handled by caller)
    // CJK Unified Ideographs Extension C
    if (cp >= 0x2A700 && cp <= 0x2B73F) return true;
    // CJK Unified Ideographs Extension D
    if (cp >= 0x2B740 && cp <= 0x2B81F) return true;
    // CJK Unified Ideographs Extension E
    if (cp >= 0x2B820 && cp <= 0x2CEAF) return true;
    // CJK Unified Ideographs Extension F
    if (cp >= 0x2CEB0 && cp <= 0x2EBEF) return true;
    // CJK Compatibility Ideographs Supplement
    if (cp >= 0x2F800 && cp <= 0x2FA1F) return true;
    // Enclosed CJK Letters and Months
    if (cp >= 0x3200 && cp <= 0x32FF) return true;
    // CJK Symbols and Punctuation (excluding 303F)
    if (cp >= 0x3000 && cp <= 0x303E) return true;
    // Hiragana
    if (cp >= 0x3041 && cp <= 0x3096) return true;
    // Katakana
    if (cp >= 0x30A1 && cp <= 0x30FF) return true;
    // Katakana Phonetic Extensions
    if (cp >= 0x31F0 && cp <= 0x31FF) return true;
    // Hangul Syllables
    if (cp >= 0xAC00 && cp <= 0xD7AF) return true;
    // Hangul Jamo Extended-A
    if (cp >= 0xA960 && cp <= 0xA97C) return true;
    // Hangul Jamo Extended-B
    if (cp >= 0xD7B0 && cp <= 0xD7FF) return true;
    // Bopomofo
    if (cp >= 0x3100 && cp <= 0x312F) return true;
    // Enclosed CJK Letters
    if (cp >= 0x3200 && cp <= 0x32FF) return true;
    // Supplemental Arrows-B (some)
    if (cp >= 0x2900 && cp <= 0x297F) return true;
    return false;
}

// Zero-width / combining characters
static bool isZeroWidth(uint32_t cp) {
    // General combining marks
    if (cp >= 0x0300 && cp <= 0x036F) return true;
    // Combining Diacritical Marks Extended
    if (cp >= 0x1AB0 && cp <= 0x1AFF) return true;
    // Combining Diacritical Marks Supplement
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return true;
    // Combining Diacritical Marks for Symbols
    if (cp >= 0x20D0 && cp <= 0x20FF) return true;
    // Combining Half Marks
    if (cp >= 0xFE20 && cp <= 0xFE2F) return true;
    // Variation Selectors
    if (cp >= 0xFE00 && cp <= 0xFE0F) return true;
    // Variation Selectors Supplement
    if (cp >= 0xE0100 && cp <= 0xE01EF) return true;
    // Zero-width format characters
    if (cp == 0x200B) return true; // Zero Width Space
    if (cp == 0x200C) return true; // Zero Width Non-Joiner
    if (cp == 0x200D) return true; // Zero Width Joiner
    if (cp == 0x200E) return true; // Left-to-Right Mark
    if (cp == 0x200F) return true; // Right-to-Left Mark
    if (cp == 0x2060) return true; // Word Joiner
    if (cp == 0x2061) return true; // Function Application
    if (cp == 0x2062) return true; // Invisible Times
    if (cp == 0x2063) return true; // Invisible Separator
    if (cp == 0x2064) return true; // Invisible Plus
    // Soft Hyphen
    if (cp == 0x00AD) return true;
    // Combining Enclosing Keycap
    if (cp == 0x20E3) return true;
    // Tag characters (used in emoji flags)
    if (cp >= 0xE0020 && cp <= 0xE007F) return true;
    return false;
}

int ucsWidth(uint32_t cp) {
    if (cp == 0) return 0;
    if (isZeroWidth(cp)) return 0;
    if (isWide(cp)) return 2;
    return 1;
}

bool isCombining(uint32_t cp) {
    return isZeroWidth(cp);
}

bool isBoxDrawing(uint32_t cp) {
    return cp >= 0x2500 && cp <= 0x257F;
}

bool isEmoji(uint32_t cp) {
    // Basic Emoji
    if (cp >= 0x1F600 && cp <= 0x1F64F) return true; // Emoticons
    if (cp >= 0x1F300 && cp <= 0x1F5FF) return true; // Misc Symbols and Pictographs
    if (cp >= 0x1F680 && cp <= 0x1F6FF) return true; // Transport and Map
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return true; // Supplemental Symbols
    if (cp >= 0x1FA00 && cp <= 0x1FA6F) return true; // Chess Symbols
    if (cp >= 0x1FA70 && cp <= 0x1FAFF) return true; // Symbols and Pictographs Extended-A
    // Miscellaneous Symbols
    if (cp >= 0x2600 && cp <= 0x26FF) return true;
    // Dingbats
    if (cp >= 0x2700 && cp <= 0x27BF) return true;
    // Regional Indicator Symbols (flags)
    if (cp >= 0x1F1E0 && cp <= 0x1F1FF) return true;
    // Keycap sequences
    if (cp >= 0x20E3) return true;
    return false;
}

bool shouldContinueGrapheme(uint32_t prev, uint32_t curr) {
    // Combining characters always continue the grapheme
    if (isCombining(curr)) return true;
    // Variation selectors continue
    if (curr >= 0xFE00 && curr <= 0xFE0F) return true;
    // ZWJ sequences (emoji)
    if (prev == 0x200D) return true;
    if (curr == 0x200D) return true;
    return false;
}
