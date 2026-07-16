# TODO: Alacritty-style glyph storage refactor

## Goal
Replace dual-atlas architecture with Alacritty-style unified lazy glyph cache
and dynamic row-based texture atlases.

## Current Problems
1. Two separate maps (`m_glyphs` + `m_fallbackGlyphs`) — redundant
2. Fixed 128-col grid atlas — wastes space, no compaction
3. `flushGlyphBatch()` binds only `m_fontTexture` — **BUG**: fallback glyphs
   reference `m_fallbackTexture` but wrong texture is bound
4. No multi-texture batch handling
5. `const_cast` hack in `findGlyph` lambda

## Target Architecture

### New Types (renderer.h)

```cpp
struct GlyphKey {
    wchar_t character;
    int fontSize;
    bool operator==(const GlyphKey& o) const {
        return character == o.character && fontSize == o.fontSize;
    }
};
// + std::hash specialization

struct Glyph {
    GLuint tex_id;
    float u0, v0, u1, v1;
    int width, height;
};

struct Atlas {
    GLuint texture = 0;
    static const int SIZE = 1024;
    int row_extent = 0;
    int row_baseline = 0;
    int row_tallest = 0;
};
```

### Member Replacements

| Remove | Add |
|--------|-----|
| `m_fontTexture` | `std::vector<Atlas> m_atlases` |
| `m_fallbackTexture` | `int m_currentAtlas = 0` |
| `m_glyphs` | `std::unordered_map<GlyphKey, Glyph> m_glyphCache` |
| `m_fallbackGlyphs` | — |
| `m_atlasTexW/H` | — |
| `m_fallbackTexW/H` | — |
| `m_fallbackAtlasRow` | — |
| `m_hasFallback` | — |
| `m_fallbackOldBmp/OldFont` | — |
| `ATLAS_COLS` | — |

Keep: `m_fallbackMemDC`, `m_fallbackBitmap`, `m_fallbackBits` (GDI rasterizer)

Add to GlyphQuad:
```cpp
struct GlyphQuad {
    float x, y, w, h;
    float u0, v0, u1, v1;
    uint32_t color;
    GLuint tex_id;  // NEW
};
```

## Implementation Steps

### Step 1: Add new types to renderer.h
- GlyphKey (with hash specialization)
- Glyph (with tex_id)
- Atlas (1024x1024, row-based)
- Update GlyphQuad (add tex_id)
- Replace member variables
- Add method declarations: `getGlyph`, `loadCommonGlyphs`, `clearGlyphCache`, `measureCellSize`

### Step 2: Implement Atlas::insertGlyph() in renderer.cpp
- Row-based bin packing algorithm:
  - `room_in_row(glyph_w, glyph_h)`: check horizontal + vertical fit
  - `advance_row()`: row_baseline += row_tallest, reset row_extent
  - If atlas full → create new Atlas (new GL texture), push to m_atlases
- Upload via `glTexSubImage2D` at (row_extent, row_baseline)
- Return `Glyph` with UV coords: `uv_left = x / 1024`, `uv_bot = y / 1024`, etc.
- Static method or standalone taking `m_atlases`, `m_currentAtlas` by ref

### Step 3: Implement getGlyph(wchar_t ch) in renderer.cpp
- Build `GlyphKey{ch, m_fontSize}`
- Cache lookup → hit → return `&glyph`
- Cache miss:
  1. GDI rasterize on `m_fallbackMemDC`: `FillRect` cell → `TextOutW` → `GetTextExtentPoint32W`
  2. Read pixels from `m_fallbackBits`, convert BGR→white-on-alpha
  3. `Atlas::insertGlyph()` → get back `Glyph` with tex_id + UV
  4. `m_glyphCache.insert(key, glyph)`
  5. Return `&glyph`
- Returns `nullptr` if glyph not renderable

### Step 4: Implement loadCommonGlyphs() in renderer.cpp
- Pre-rasterize ASCII 32-126 (95 glyphs)
- Loop: `for (wchar_t c = 0x20; c < 0x7F; c++) getGlyph(c);`
- Called from `init()` and `setFontSize()`

### Step 5: Implement clearGlyphCache() in renderer.cpp
- `m_glyphCache.clear()`
- For each atlas in `m_atlases`: reset row_extent/row_baseline/row_tallest to 0
  (reuse GL textures, don't delete)
- Reset `m_currentAtlas = 0`

### Step 6: Implement measureCellSize() in renderer.cpp
- Create font: `CreateFontW(-m_fontSize, ...)`
- Measure: `GetTextMetricsW` + `GetTextExtentPoint32W("X")`
- Set `m_cellWidth`, `m_cellHeight`
- Delete font
- Fast (~microseconds), no caching needed

### Step 7: Update drawCell() in renderer.cpp
- Replace `findGlyph` lambda with single call:
  ```cpp
  GlyphKey key = {ch, m_fontSize};
  auto it = m_glyphCache.find(key);
  const Glyph* gi = (it != m_glyphCache.end())
      ? &it->second
      : const_cast<Renderer*>(this)->getGlyph(ch);
  ```
- Use `gi->tex_id` when building GlyphQuad
- Same logic for combined characters

### Step 8: Fix flushGlyphBatch() in renderer.cpp
- Track current bound texture
- Rebind when `tex_id` changes:
  ```cpp
  GLuint lastTex = 0;
  for (const auto& q : m_glyphBatch) {
      if (q.tex_id != lastTex) {
          if (lastTex != 0) glEnd();
          glBindTexture(GL_TEXTURE_2D, q.tex_id);
          if (lastTex == 0) glEnable(GL_TEXTURE_2D);
          glBegin(GL_QUADS);
          lastTex = q.tex_id;
      }
      // draw quad
  }
  if (lastTex != 0) glEnd();
  ```

### Step 9: Rewrite init() in renderer.cpp
```
1. GL setup (pixel format, context, blend)  — unchanged
2. measureCellSize()
3. Create first Atlas (glGenTextures + glTexImage2D 1024x1024)
4. Create GDI scratchpad (m_fallbackMemDC + m_fallbackBitmap)
5. loadCommonGlyphs()
6. m_initialized = true
```

### Step 10: Rewrite setFontSize() in renderer.cpp
```
1. Clear old GDI scratchpad (m_fallbackBitmap/m_fallbackMemDC)
2. m_fontSize = size
3. measureCellSize()
4. clearGlyphCache()
5. Recreate GDI scratchpad for new cell dimensions
6. loadCommonGlyphs()
```

### Step 11: Clean up renderer.h and renderer.cpp
- Remove: `createFontAtlas`, `createFallbackAtlas`, `addGlyphToAtlas`, `renderCombinedGlyphs`
- Remove all dead member variables
- Clean up `shutdown()`: delete all atlas GL textures + GDI objects

### Step 12: Update main.cpp
- `WM_SIZE`: already has `InvalidateRect` — no change needed
- Font size keybindings: already call `setFontSize()` directly — no change needed
- Remove debounce timer remnants (if any still exist)

## Verification
- Build with `cmake --build build`
- Test: change font size with Ctrl+Shift+/- — should be instant
- Test: resize window — text should remain visible
- Test: display CJK/emoji characters — should render via lazy loading
- Test: hold Ctrl+Shift+/- — rapid size changes without freeze

## Risk Notes
- The `const_cast` in drawCell should be cleaned up (getGlyph should not be const, or make glyphCache mutable)
- Atlas full scenario: silently drops glyphs currently — consider logging warning
- GDI scratchpad (m_fallbackBits) needs to be recreated on font size change since cell dimensions change
