#pragma once

#include <cstdint>

// Returns the number of terminal cells occupied by a codepoint.
// 0 = combining character (zero-width)
// 1 = normal character
// 2 = wide character (CJK, fullwidth, etc.)
int ucsWidth(uint32_t cp);

// Check if a codepoint is a combining character (zero-width)
bool isCombining(uint32_t cp);

// Check if a codepoint is a box-drawing character
bool isBoxDrawing(uint32_t cp);

// Check if a codepoint is an emoji (needs special handling)
bool isEmoji(uint32_t cp);

// Grapheme cluster state machine (UAX#29)
// Returns true if the next codepoint should be combined with the current one
bool shouldContinueGrapheme(uint32_t prev, uint32_t curr);
