#pragma once

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <string>

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(GLFWwindow* window);
    void shutdown();

    void beginFrame(int fbWidth, int fbHeight, int winWidth, int winHeight);
    void flushBatches();
    void present();
    void endFrame();

    void drawCell(int x, int y, wchar_t ch, uint32_t fg, uint32_t bg, bool bold);
    void drawCursor(int x, int y, int cellW, int cellH, uint32_t color);

    int getCellWidth() const { return m_cellWidth; }
    int getCellHeight() const { return m_cellHeight; }
    bool isInitialized() const { return m_initialized; }

private:
    bool createFontAtlas(int fbW, int fbH, int winW, int winH);

    GLFWwindow* m_window = nullptr;
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
