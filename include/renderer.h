#pragma once

#include <windows.h>
#include <GL/gl.h>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cstdio>

struct Cell;

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
    bool createFontAtlas(HDC hdc);
    bool createFallbackAtlas(HDC hdc);
    void addGlyphToAtlas(HDC hdc, HFONT hFont, wchar_t ch);
    void renderCombinedGlyphs(HDC hdc, HFONT hFont, int x, int y, const Cell& cell, uint32_t fg);

    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hglrc = nullptr;
    bool m_initialized = false;

    GLuint m_fontTexture = 0;
    int m_cellWidth = 0;
    int m_cellHeight = 0;
    int m_fontSize = 16;

    struct GlyphInfo {
        float u0, v0, u1, v1;
        int width, height;
    };
    std::unordered_map<wchar_t, GlyphInfo> m_glyphs;

    int m_atlasTexW = 0;
    int m_atlasTexH = 0;
    static const int ATLAS_COLS = 128;

    GLuint m_fallbackTexture = 0;
    int m_fallbackTexW = 0;
    int m_fallbackTexH = 0;
    std::unordered_map<wchar_t, GlyphInfo> m_fallbackGlyphs;
    bool m_hasFallback = false;
    int m_fallbackAtlasRow = 0;
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
    };
    std::vector<BgQuad> m_bgBatch;
    std::vector<GlyphQuad> m_glyphBatch;
    void flushBgBatch();
    void flushGlyphBatch();
};

void renderer_set_log_file(FILE* f);
