#include "unicode.h"
#include <algorithm>

// Unicode character ranges and their widths
// Based on: https://www.unicode.org/Public/UCD/latest/ucd/EastAsianWidth.txt
// and https://www.unicode.org/Public/UCD/latest/ucd/DerivedGeneralCategory.txt

// Width: -1 = control/unassigned, 0 = combining/zero-width, 1 = narrow, 2 = wide

// Combining characters (General Category: Mn, Mc, Me)
// These have width 0
static bool is_combining_range(uint32_t cp) {
    // Mn (Nonspacing Marks)
    if ((cp >= 0x0300 && cp <= 0x036F) ||   // Combining Diacritical Marks
        (cp >= 0x0483 && cp <= 0x0489) ||   // Combining Cyrillic
        (cp >= 0x064B && cp <= 0x065F) ||   // Arabic combining
        (cp >= 0x0670 && cp <= 0x0670) ||   // Arabic letter superscript alef
        (cp >= 0x06D6 && cp <= 0x06DC) ||   // Arabic small letters
        (cp >= 0x06DF && cp <= 0x06ED) ||   // Arabic small signs
        (cp >= 0x0711 && cp <= 0x0711) ||   // Syriac combining
        (cp >= 0x0730 && cp <= 0x074A) ||   // Syriac combining
        (cp >= 0x07A6 && cp <= 0x07B0) ||   // Thaana combining
        (cp >= 0x07EB && cp <= 0x07F5) ||   // NKo combining
        (cp >= 0x0890 && cp <= 0x089F) ||   // Devanagari combining
        (cp >= 0x08E4 && cp <= 0x094D) ||   // Various Indic combining
        (cp >= 0x0951 && cp <= 0x0957) ||   // Devanagari stress signs
        (cp >= 0x09CE && cp <= 0x09CE) ||   // Bengali vowel sign
        (cp >= 0x0A3C && cp <= 0x0A3C) ||   // Gujarati vowel sign
        (cp >= 0x0ABC && cp <= 0x0ABC) ||   // Gujarati sign
        (cp >= 0x0B3C && cp <= 0x0B3C) ||   // Oriya sign
        (cp >= 0x0BD7 && cp <= 0x0BD7) ||   // Tamil au length mark
        (cp >= 0x0C3E && cp <= 0x0C56) ||   // Telugu combining
        (cp >= 0x0CBC && cp <= 0x0CC4) ||   // Kannada combining
        (cp >= 0x0CE0 && cp <= 0x0CE1) ||   // Kannada combining
        (cp >= 0x0D41 && cp <= 0x0D44) ||   // Malayalam combining
        (cp >= 0x0D4D && cp <= 0x0D4D) ||   // Malayalam sign
        (cp >= 0x0D5F && cp <= 0x0D63) ||   // Malayalam combining
        (cp >= 0x0DCA && cp <= 0x0DD4) ||   // Sinhala combining
        (cp >= 0x0DD8 && cp <= 0x0DDF) ||   // Sinhala combining
        (cp >= 0x0F01 && cp <= 0x0F03) ||   // Tibetan marks
        (cp >= 0x0F18 && cp <= 0x0F19) ||   // Tibetan astrological signs
        (cp >= 0x0F35 && cp <= 0x0F35) ||   // Tibetan mark
        (cp >= 0x0F37 && cp <= 0x0F37) ||   // Tibetan mark
        (cp >= 0x0F39 && cp <= 0x0F39) ||   // Tibetan mark
        (cp >= 0x0F71 && cp <= 0x0F84) ||   // Tibetan vowel signs
        (cp >= 0x102B && cp <= 0x103E) ||   // Myanmar combining
        (cp >= 0x1056 && cp <= 0x1059) ||   // Myanmar vowel signs
        (cp >= 0x105E && cp <= 0x1060) ||   // Myanmar tone marks
        (cp >= 0x1062 && cp <= 0x1064) ||   // Myanmar signs
        (cp >= 0x1065 && cp <= 0x1070) ||   // Myanmar combining
        (cp >= 0x1082 && cp <= 0x108D) ||   // Myanmar consonant signs
        (cp >= 0x108F && cp <= 0x108F) ||   // Myanmar sign
        (cp >= 0x135F && cp <= 0x135F) ||   // Ethiopic combining
        (cp >= 0x1712 && cp <= 0x1714) ||   // Tagalog signs
        (cp >= 0x1732 && cp <= 0x1733) ||   // Hanunoo signs
        (cp >= 0x1752 && cp <= 0x1753) ||   // Buhid signs
        (cp >= 0x1772 && cp <= 0x1773) ||   // Tagbanwa signs
        (cp >= 0x17B4 && cp <= 0x17D3) ||   // Khmer combining
        (cp >= 0x17DD && cp <= 0x17DD) ||   // Khmer independent vowel
        (cp >= 0x18A9 && cp <= 0x18A9) ||   // Mongolian free variation selector
        (cp >= 0x1920 && cp <= 0x1945) ||   // Limbu combining
        (cp >= 0x1A17 && cp <= 0x1A1B) ||   // Buginese vowel signs
        (cp >= 0x1AA7 && cp <= 0x1AA7) ||   // Tai Tham sign
        (cp >= 0x1B00 && cp <= 0x1B04) ||   // Balinese signs
        (cp >= 0x1B05 && cp <= 0x1B33) ||   // Balinese combining
        (cp >= 0x1B34 && cp <= 0x1B44) ||   // Balinese combining
        (cp >= 0x1B6B && cp <= 0x1B73) ||   // Balinese musical signs
        (cp >= 0x1B80 && cp <= 0x1B82) ||   // Sundanese combining
        (cp >= 0x1BA1 && cp <= 0x1BAD) ||   // Sundanese combining
        (cp >= 0x1BE6 && cp <= 0x1BF3) ||   // Batak combining
        (cp >= 0x1C23 && cp <= 0x1C37) ||   // Lepcha combining
        (cp >= 0x1CD0 && cp <= 0x1CFA) ||   // Vedic combining
        (cp >= 0x1DC0 && cp <= 0x1DFF) ||   // Combining Diacritical Marks Supplement
        (cp >= 0x20D0 && cp <= 0x20FF) ||   // Combining Diacritical Marks for Symbols
        (cp >= 0x2CEF && cp <= 0x2CF1) ||   // Coptic combining
        (cp >= 0x2D7F && cp <= 0x2D7F) ||   // Tifinagh modifier letter
        (cp >= 0x2DE0 && cp <= 0x2DFF) ||   // Combining Cyrillic
        (cp >= 0x302A && cp <= 0x302F) ||   // Bopomofo combining
        (cp >= 0x3099 && cp <= 0x309A) ||   // CJK combining
        (cp >= 0xA66F && cp <= 0xA672) ||   // Combining Cyrillic
        (cp >= 0xA674 && cp <= 0xA67D) ||   // Combining Latin
        (cp >= 0xA6F0 && cp <= 0xA6F1) ||   // Bamum combining
        (cp >= 0xA802 && cp <= 0xA802) ||   // Syloti Nagri sign
        (cp >= 0xA806 && cp <= 0xA822) ||   // Syloti Nagri combining
        (cp >= 0xA840 && cp <= 0xA873) ||   // Phags-pa combining
        (cp >= 0xA880 && cp <= 0xA8C4) ||   // Saurastra combining
        (cp >= 0xA8E0 && cp <= 0xA8F1) ||   // Devanagari combining
        (cp >= 0xA926 && cp <= 0xA92D) ||   // Kayah Li combining
        (cp >= 0xA947 && cp <= 0xA953) ||   // Rejang combining
        (cp >= 0xA980 && cp <= 0xA9B2) ||   // Javanese combining
        (cp >= 0xA9B3 && cp <= 0xA9C0) ||   // Javanese combining
        (cp >= 0xA9E5 && cp <= 0xA9E5) ||   // Myanmar combining
        (cp >= 0xAA29 && cp <= 0xAA2E) ||   // Cham combining
        (cp >= 0xAA43 && cp <= 0xAA43) ||   // Cham combining
        (cp >= 0xAA4C && cp <= 0xAA4D) ||   // Cham combining
        (cp >= 0xAA7C && cp <= 0xAA7C) ||   // Myanmar combining
        (cp >= 0xAAB0 && cp <= 0xAAB0) ||   // Tai Viet combining
        (cp >= 0xAAB2 && cp <= 0xAAB4) ||   // Tai Viet combining
        (cp >= 0xAAB7 && cp <= 0xAABE) ||   // Tai Viet combining
        (cp >= 0xAAC1 && cp <= 0xAAC1) ||   // Tai Viet combining
        (cp >= 0xAAEC && cp <= 0xAAED) ||   // Meetei Mayek combining
        (cp >= 0xAAF6 && cp <= 0xAAF6) ||   // Meetei Mayek combining
        (cp >= 0xABE2 && cp <= 0xABE2) ||   // Meetei Mayek combining
        (cp >= 0xFB1E && cp <= 0xFB1E) ||   // Hebrew combining
        (cp >= 0xFE00 && cp <= 0xFE0F) ||   // Variation Selectors
        (cp >= 0xFE20 && cp <= 0xFE2F) ||   // Combining Half Marks
        (cp >= 0x101FD && cp <= 0x101FD) ||   // Greek combining
        (cp >= 0x102E0 && cp <= 0x102E0) ||   // Coptic combining
        (cp >= 0x10376 && cp <= 0x1037A) ||   // Old Persian combining
        (cp >= 0x10A01 && cp <= 0x10A03) ||   // Kharoshthi combining
        (cp >= 0x10A05 && cp <= 0x10A06) ||   // Kharoshthi combining
        (cp >= 0x10A0C && cp <= 0x10A13) ||   // Kharoshthi combining
        (cp >= 0x10A38 && cp <= 0x10A3A) ||   // Old South Arabian combining
        (cp >= 0x10A3F && cp <= 0x10A3F) ||   // Old South Arabian combining
        (cp >= 0x10AEB && cp <= 0x10AEB) ||   // Manichaean combining
        (cp >= 0x10AF1 && cp <= 0x10AF4) ||   // Manichaean combining
        (cp >= 0x10B39 && cp <= 0x10B3F) ||   // Inscriptional Parthian combining
        (cp >= 0x10B99 && cp <= 0x10B9C) ||   // Inscriptional Pahlavi combining
        (cp >= 0x10C80 && cp <= 0x10CFA) ||   // Psalter Pahlavi combining
        (cp >= 0x10D30 && cp <= 0x10D39) ||   // Hanifi Rohingya combining
        (cp >= 0x10EAB && cp <= 0x10EAC) ||   // Yezidi combining
        (cp >= 0x10F46 && cp <= 0x10F50) ||   // Old Sogdian combining
        (cp >= 0x10FC5 && cp <= 0x10FC5) ||   // Sogdian combining
        (cp >= 0x11038 && cp <= 0x11046) ||   // Brahmi combining
        (cp >= 0x11070 && cp <= 0x11070) ||   // Brahmi combining
        (cp >= 0x11073 && cp <= 0x11074) ||   // Brahmi combining
        (cp >= 0x1107F && cp <= 0x11082) ||   // Kaithi combining
        (cp >= 0x110B0 && cp <= 0x110BA) ||   // Sora Sompeng combining
        (cp >= 0x11100 && cp <= 0x11102) ||   // Chakma combining
        (cp >= 0x11127 && cp <= 0x11134) ||   // Chakma combining
        (cp >= 0x11173 && cp <= 0x11173) ||   // Mahajani combining
        (cp >= 0x11180 && cp <= 0x11182) ||   // Sharada combining
        (cp >= 0x111B3 && cp <= 0x111C0) ||   // Sharada combining
        (cp >= 0x111CA && cp <= 0x111CC) ||   // Sinhala archaic combining
        (cp >= 0x1122C && cp <= 0x11234) ||   // Khojki combining
        (cp >= 0x11235 && cp <= 0x11236) ||   // Khojki combining
        (cp >= 0x11238 && cp <= 0x11241) ||   // Khojki combining
        (cp >= 0x112E0 && cp <= 0x112E2) ||   // Khudawadi combining
        (cp >= 0x11300 && cp <= 0x11303) ||   // Grantha combining
        (cp >= 0x1133B && cp <= 0x1133C) ||   // Grantha combining
        (cp >= 0x1133E && cp <= 0x11350) ||   // Grantha combining
        (cp >= 0x11357 && cp <= 0x11363) ||   // Grantha combining
        (cp >= 0x11366 && cp <= 0x1136C) ||   // Grantha combining
        (cp >= 0x11370 && cp <= 0x11374) ||   // Grantha combining
        (cp >= 0x114B0 && cp <= 0x114C3) ||   // Tirhuta combining
        (cp >= 0x114D0 && cp <= 0x114D9) ||   // Siddham combining
        (cp >= 0x11580 && cp <= 0x115FF) ||   // Sidham combining
        (cp >= 0x11630 && cp <= 0x11644) ||   // Modi combining
        (cp >= 0x11650 && cp <= 0x11659) ||   // Mongolian combining
        (cp >= 0x11680 && cp <= 0x116B8) ||   // Takri combining
        (cp >= 0x11710 && cp <= 0x1172B) ||   // Ahom combining
        (cp >= 0x1182C && cp <= 0x11840) ||   // Dogra combining
        (cp >= 0x118EA && cp <= 0x11900) ||   // Warang Citi combining
        (cp >= 0x11930 && cp <= 0x11935) ||   // Dives Akuru combining
        (cp >= 0x11937 && cp <= 0x11946) ||   // Dives Akuru combining
        (cp >= 0x11A01 && cp <= 0x11A3A) ||   // Zanabazar Square combining
        (cp >= 0x11A50 && cp <= 0x11A99) ||   // Soyombo combining
        (cp >= 0x11AC0 && cp <= 0x11AF8) ||   // Pau Cin Hau combining
        (cp >= 0x11C2F && cp <= 0x11C36) ||   // Bhaiksuki combining
        (cp >= 0x11C38 && cp <= 0x11C40) ||   // Bhaiksuki combining
        (cp >= 0x11C5A && cp <= 0x11C6C) ||   // Marchen combining
        (cp >= 0x11C8E && cp <= 0x11C91) ||   // Masaram Gondi combining
        (cp >= 0x11D30 && cp <= 0x11D4A) ||   // Gunjala Gondi combining
        (cp >= 0x11D50 && cp <= 0x11D59) ||   // Makasar combining
        (cp >= 0x11FB0 && cp <= 0x11FB0) ||   // Lisu combining
        (cp >= 0x16E40 && cp <= 0x16E9A) ||   // Tangut combining
        (cp >= 0x1D165 && cp <= 0x1D169) ||   // Musical symbols combining
        (cp >= 0x1D16D && cp <= 0x1D17A) ||   // Musical symbols combining
        (cp >= 0x1E000 && cp <= 0x1E02B) ||   // Glagolitic combining
        (cp >= 0x1E137 && cp <= 0x1E149) ||   // Nykia combining
        (cp >= 0x1E2F0 && cp <= 0x1E2F9) ||   // Toto combining
        (cp >= 0x1E7E6 && cp <= 0x1E7EB) ||   // Ethiopic combining
        (cp >= 0x1E8D0 && cp <= 0x1E8DF) ||   // Mende combining
        (cp >= 0x1E944 && cp <= 0x1E94A) ||   // Adlam combining
        (cp >= 0xE0100 && cp <= 0xE01EF)) {  // Variation Selectors Supplement
        return true;
    }
    return false;
}

// Wide characters (East Asian Fullwidth, Halfwidth Katakana, etc.)
// These have width 2
static bool is_wide_range(uint32_t cp) {
    // CJK Unified Ideographs
    if ((cp >= 0x4E00 && cp <= 0x9FFF) ||
        // CJK Unified Ideographs Extension A
        (cp >= 0x3400 && cp <= 0x4DBF) ||
        // CJK Unified Ideographs Extension B
        (cp >= 0x20000 && cp <= 0x2A6DF) ||
        (cp >= 0x2A700 && cp <= 0x2B73F) ||
        (cp >= 0x2B740 && cp <= 0x2B81F) ||
        (cp >= 0x2B820 && cp <= 0x2CEAF) ||
        // CJK Compatibility Ideographs
        (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0x2F800 && cp <= 0x2FA1F) ||
        // CJK Radicals Supplement
        (cp >= 0x2E80 && cp <= 0x2EFF) ||
        // Kangxi Radicals
        (cp >= 0x2F00 && cp <= 0x2FDF) ||
        // CJK Symbols and Punctuation
        (cp >= 0x3000 && cp <= 0x303F) ||
        // Hiragana
        (cp >= 0x3040 && cp <= 0x309F) ||
        // Katakana
        (cp >= 0x30A0 && cp <= 0x30FF) ||
        // Bopomofo
        (cp >= 0x3100 && cp <= 0x312F) ||
        // Hangul Compatibility Jamo
        (cp >= 0x3130 && cp <= 0x318F) ||
        // Kanbun
        (cp >= 0x3190 && cp <= 0x319F) ||
        // Bopomofo Extended
        (cp >= 0x31A0 && cp <= 0x31BF) ||
        // Katakana Phonetic Extensions
        (cp >= 0x31F0 && cp <= 0x31FF) ||
        // Enclosed CJK Letters and Months
        (cp >= 0x3200 && cp <= 0x32FF) ||
        // CJK Compatibility
        (cp >= 0x3300 && cp <= 0x33FF) ||
        // CJK Unified Ideographs Extension C-F
        (cp >= 0x2EB00 && cp <= 0x2EBEF) ||
        (cp >= 0x2EC00 && cp <= 0x2EC7F) ||
        (cp >= 0x2ED00 && cp <= 0x2ED2F) ||
        // Hangul Syllables
        (cp >= 0xAC00 && cp <= 0xD7AF) ||
        // Hangul Jamo
        (cp >= 0x1100 && cp <= 0x11FF) ||
        // Hangul Jamo Extended-A
        (cp >= 0xA960 && cp <= 0xA97F) ||
        // Hangul Jamo Extended-B
        (cp >= 0xD7B0 && cp <= 0xD7FF) ||
        // Halfwidth and Fullwidth Forms (fullwidth characters)
        (cp >= 0xFF00 && cp <= 0xFFEF) ||
        // Ideographic Description Characters
        (cp >= 0x2FF0 && cp <= 0x2FFF) ||
        // CJK Strokes
        (cp >= 0x31C0 && cp <= 0x31EF) ||
        // Enclosed Ideographic Supplement
        (cp >= 0x1F200 && cp <= 0x1F2FF) ||
        // Miscellaneous Symbols and Pictographs (some wide emoji)
        (cp >= 0x1F300 && cp <= 0x1F5FF) ||
        (cp >= 0x1F600 && cp <= 0x1F64F) ||
        (cp >= 0x1F680 && cp <= 0x1F6FF) ||
        (cp >= 0x1F700 && cp <= 0x1F77F) ||
        (cp >= 0x1F780 && cp <= 0x1F7FF) ||
        (cp >= 0x1F800 && cp <= 0x1F8FF) ||
        (cp >= 0x1F900 && cp <= 0x1F9FF) ||
        (cp >= 0x1FA00 && cp <= 0x1FA6F) ||
        (cp >= 0x1FA70 && cp <= 0x1FAFF) ||
        // Tags
        (cp >= 0xE0000 && cp <= 0xE007F) ||
        // Variation Selectors (but these are zero-width)
        // Yijing Hexagram Symbols
        (cp >= 0x2600 && cp <= 0x26FF) ||
        // Miscellaneous Technical
        (cp >= 0x2300 && cp <= 0x23FF) ||
        // Control Pictures
        (cp >= 0x2400 && cp <= 0x243F) ||
        // Optical Character Recognition
        (cp >= 0x2440 && cp <= 0x245F) ||
        // Dingbats
        (cp >= 0x2700 && cp <= 0x27BF) ||
        // Miscellaneous Mathematical Symbols-A
        (cp >= 0x27C0 && cp <= 0x27EF) ||
        // Supplemental Arrows-A
        (cp >= 0x27F0 && cp <= 0x27FF) ||
        // Supplemental Arrows-B
        (cp >= 0x2900 && cp <= 0x297F) ||
        // Miscellaneous Mathematical Symbols-B
        (cp >= 0x2980 && cp <= 0x29FF) ||
        // Supplemental Mathematical Operators
        (cp >= 0x2A00 && cp <= 0x2AFF) ||
        // Miscellaneous Symbols and Arrows
        (cp >= 0x2B00 && cp <= 0x2BFF)) {
        return true;
    }
    return false;
}

// Zero-width characters (control, format, private use, etc.)
static bool is_zero_width(uint32_t cp) {
    // Control characters (C0, C1, DEL)
    if (cp <= 0x1F || (cp >= 0x7F && cp <= 0x9F)) {
        return true;
    }
    // Format characters (Cf category)
    if ((cp >= 0x200C && cp <= 0x200F) ||   // Zero width space, non-joiner, joiner
        (cp >= 0x2028 && cp <= 0x2029) ||   // Line separator, paragraph separator
        (cp >= 0x202A && cp <= 0x202E) ||   // Various space separators
        (cp >= 0x2060 && cp <= 0x2064) ||   // Word joiner, invisible separator, etc.
        (cp >= 0x2066 && cp <= 0x206F) ||   // Various format controls
        // Tag characters
        (cp >= 0xE0000 && cp <= 0xE007F) ||
        // Variation selectors
        (cp >= 0xFE00 && cp <= 0xFE0F) ||
        (cp >= 0xE0100 && cp <= 0xE01EF) ||
        // Private Use Area
        (cp >= 0xE000 && cp <= 0xF8FF) ||
        (cp >= 0xF0000 && cp <= 0xFFFFF) ||
        (cp >= 0x100000 && cp <= 0x10FFFD)) {
        return true;
    }
    return false;
}

int unicode_width(uint32_t codepoint) {
    // Check for zero-width (control, format, etc.)
    if (is_zero_width(codepoint)) {
        return -1;  // Control/unassigned
    }
    
    // Check for combining characters
    if (is_combining_range(codepoint)) {
        return 0;
    }
    
    // Check for wide characters (East Asian)
    if (is_wide_range(codepoint)) {
        return 2;
    }
    
    // Default: narrow (width 1)
    return 1;
}

bool is_combining(uint32_t codepoint) {
    return is_combining_range(codepoint);
}

bool is_regional_indicator(uint32_t codepoint) {
    return codepoint >= 0x1F1E6 && codepoint <= 0x1F1FF;
}

bool is_variation_selector(uint32_t codepoint) {
    return (codepoint >= 0xFE00 && codepoint <= 0xFE0F) ||
           (codepoint >= 0xE0100 && codepoint <= 0xE01EF);
}

bool is_emoji_presentation(uint32_t codepoint) {
    // Most emoji are in these ranges
    if ((codepoint >= 0x1F300 && codepoint <= 0x1F5FF) ||
        (codepoint >= 0x1F600 && codepoint <= 0x1F64F) ||
        (codepoint >= 0x1F680 && codepoint <= 0x1F6FF) ||
        (codepoint >= 0x1F700 && codepoint <= 0x1F77F) ||
        (codepoint >= 0x1F780 && codepoint <= 0x1F7FF) ||
        (codepoint >= 0x1F800 && codepoint <= 0x1F8FF) ||
        (codepoint >= 0x1F900 && codepoint <= 0x1F9FF) ||
        (codepoint >= 0x1FA00 && codepoint <= 0x1FA6F) ||
        (codepoint >= 0x1FA70 && codepoint <= 0x1FAFF) ||
        // Symbols that are often rendered as emoji
        (codepoint >= 0x2600 && codepoint <= 0x26FF) ||
        (codepoint >= 0x2700 && codepoint <= 0x27BF) ||
        // Transport and map symbols
        (codepoint >= 0x1F680 && codepoint <= 0x1F6FF) ||
        // Flags (Regional Indicator symbols)
        (codepoint >= 0x1F1E6 && codepoint <= 0x1F1FF)) {
        return true;
    }
    return false;
}

uint32_t get_emoji_base(uint32_t codepoint) {
    // If it's a variation selector, return the base
    if (is_variation_selector(codepoint)) {
        // This is a simplified approach - in reality we'd need to track the base
        return 0;
    }
    // For zwj sequences and flags, we'd need more context
    // For now, just return the codepoint itself
    return codepoint;
}
