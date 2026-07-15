#pragma once

#include <cstdint>
#include <cwchar>

// Unicode character width according to WCIDTH
// Returns: 0 (combining), 1 (normal), 2 (wide), -1 (control/unspecified)
int unicode_width(uint32_t codepoint);

// Grapheme cluster segmentation state for UAX#29
struct GraphemeState {
    uint32_t prev_class = 0;
    bool expect_combining = false;
};

// Check if codepoint is a combining mark (Mn, Mc, Me)
bool is_combining(uint32_t codepoint);

// Check if codepoint is a regional indicator (for emoji flags)
bool is_regional_indicator(uint32_t codepoint);

// Check if codepoint is a variation selector (VS15, VS16 for emoji)
bool is_variation_selector(uint32_t codepoint);

// Check if codepoint should be treated as emoji
bool is_emoji_presentation(uint32_t codepoint);

// Get the base codepoint for emoji with variation selectors
uint32_t get_emoji_base(uint32_t codepoint);
