#pragma once

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include <GLFW/glfw3.h>
#include "shader.h"
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
    void drawCell(int x, int y, wchar_t ch, uint32_t fg, uint32_t bg, bool bold, bool italic);
    void drawCursor(int x, int y, int cellW, int cellH, uint32_t color);
    void flushBatches();
    void present();

    int getCellWidth() const { return m_cellWidth; }
    int getCellHeight() const { return m_cellHeight; }
    bool isInitialized() const { return m_initialized; }

private:
    bool createFontAtlas(int fbW, int fbH, int winW, int winH, int fontStyle);
    bool createShaders();

    GLFWwindow* m_window = nullptr;
    bool m_initialized = false;

    static const int NUM_STYLES = 4;
    GLuint m_fontTexture[NUM_STYLES] = {0};
    int m_cellWidth = 0;
    int m_cellHeight = 0;

    struct GlyphInfo {
        float u0, v0, u1, v1;
        float bearX, bearY;
        float glyphW, glyphH;
        bool isColor = false;
    };
    std::unordered_map<wchar_t, GlyphInfo> m_glyphs[NUM_STYLES];

    int m_atlasTexW = 0;
    int m_atlasTexH = 0;
    static const int ATLAS_COLS = 128;

    // GL3 state
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    // Shaders with cached uniform locations
    ShaderProgram m_textShader;
    GLint m_textProjLoc = -1;
    GLint m_textCellSizeLoc = -1;
    GLint m_textPassLoc = -1;
    GLint m_textMaskLoc = -1;

    ShaderProgram m_rectShader;
    GLint m_rectProjLoc = -1;

    // Text instance: 24 bytes packed
    // cell_flags bits: 0=isColor(emoji), 1-2=fontStyle(0-3)
    struct TextInstance {
        int16_t col, row;
        int16_t left, top, width, height;
        float uv_left, uv_bot, uv_width, uv_height;
        uint8_t r, g, b, cell_flags;
        uint8_t bg_r, bg_g, bg_b, bg_a;
    };
    static_assert(sizeof(TextInstance) == 36, "TextInstance must be 36 bytes");

    std::vector<TextInstance> m_textBatch;
    static const int BATCH_MAX = 65536;

    // Rect instance: 32 bytes
    struct RectInstance {
        float x, y, w, h;
        float r, g, b, a;
    };
    GLuint m_rectVao = 0;
    GLuint m_rectVbo = 0;
    GLuint m_rectEbo = 0;
    GLuint m_rectInstanceVbo = 0;
    std::vector<RectInstance> m_rectBatch;
    std::vector<RectInstance> m_cursorBatch;

    // Projection
    float m_projScaleX = 1.0f;
    float m_projScaleY = 1.0f;
    float m_projOffsetX = 0.0f;
    float m_projOffsetY = 0.0f;

    int m_fbWidth = 0;
    int m_fbHeight = 0;
    int m_winWidth = 0;
    int m_winHeight = 0;

    void flushTextBatch();
    void flushRectBatch();
};

void renderer_set_log_file(FILE* f);
