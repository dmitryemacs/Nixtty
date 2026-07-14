#pragma once

#include <windows.h>
#include <GL/gl.h>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cstdio>

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(HWND hwnd);
    void shutdown();

    void beginFrame(int width, int height);
    void endFrame();

    void drawCell(int x, int y, wchar_t ch, uint32_t fg, uint32_t bg, bool bold);
    void drawCursor(int x, int y, int cellW, int cellH, uint32_t color);

    int getCellWidth() const { return m_cellWidth; }
    int getCellHeight() const { return m_cellHeight; }
    bool isInitialized() const { return m_initialized; }

private:
    bool createFontAtlas(HDC hdc);

    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hglrc = nullptr;
    bool m_initialized = false;

    GLuint m_fontTexture = 0;
    int m_cellWidth = 0;
    int m_cellHeight = 0;

    struct GlyphInfo {
        float u0, v0, u1, v1;
        int width, height;
    };
    std::unordered_map<wchar_t, GlyphInfo> m_glyphs;

    int m_atlasTexW = 0;
    int m_atlasTexH = 0;
    static const int ATLAS_COLS = 128;
};

void renderer_set_log_file(FILE* f);
