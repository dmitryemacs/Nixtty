#pragma once

#include <windows.h>
#include <GL/gl.h>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdio>

struct Cell;

struct GlyphKey {
    wchar_t character;
    int fontSize;
    bool operator==(const GlyphKey& o) const {
        return character == o.character && fontSize == o.fontSize;
    }
};

namespace std {
    template<> struct hash<GlyphKey> {
        size_t operator()(const GlyphKey& k) const {
            size_t h1 = hash<wchar_t>()(k.character);
            size_t h2 = hash<int>()(k.fontSize);
            return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}

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

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(HWND hwnd);
    void shutdown();

    void beginFrame(int width, int height);
    void flushBatches();
    void present();
    void endFrame();

    void drawCell(int x, int y, const Cell& cell, uint32_t bg);
    void drawCursor(int x, int y, int cellW, int cellH, uint32_t color);

    int getCellWidth() const { return m_cellWidth; }
    int getCellHeight() const { return m_cellHeight; }
    bool isInitialized() const { return m_initialized; }

    void setFontSize(int size);
    int getFontSize() const { return m_fontSize; }

private:
    Glyph* getGlyph(wchar_t ch);
    Glyph insertGlyph(int glyph_w, int glyph_h, const uint8_t* rgba_data);
    void loadCommonGlyphs();
    void clearGlyphCache();
    void measureCellSize();
    void createGDIScratchpad(HDC hdc, HFONT hFont);
    void destroyGDIScratchpad();

    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hglrc = nullptr;
    bool m_initialized = false;

    int m_cellWidth = 0;
    int m_cellHeight = 0;
    int m_fontSize = 16;

    std::vector<Atlas> m_atlases;
    int m_currentAtlas = 0;
    std::unordered_map<GlyphKey, Glyph> m_glyphCache;

    HDC m_fallbackMemDC = nullptr;
    HGDIOBJ m_fallbackOldBmp = nullptr;
    HGDIOBJ m_fallbackOldFont = nullptr;
    void* m_fallbackBits = nullptr;
    HBITMAP m_fallbackBitmap = nullptr;

    struct BgQuad {
        float x, y, w, h;
        uint32_t color;
    };
    struct GlyphQuad {
        float x, y, w, h;
        float u0, v0, u1, v1;
        uint32_t color;
        GLuint tex_id;
    };
    std::vector<BgQuad> m_bgBatch;
    std::vector<GlyphQuad> m_glyphBatch;
    void flushBgBatch();
    void flushGlyphBatch();
};

void renderer_set_log_file(FILE* f);
