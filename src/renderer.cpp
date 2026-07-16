#include "renderer.h"
#include "terminal.h"
#include "unicode.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static const wchar_t* FONT_NAMES[] = {
    L"Hack Nerd Font Mono",
    L"Hack Nerd Font",
    L"Consolas",
    L"Courier New",
};

static FILE* r_log = nullptr;
void renderer_set_log_file(FILE* f) { r_log = f; }

static void dbg(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
    if (r_log) { fprintf(r_log, "[RND] %s", buf); fflush(r_log); }
}

static inline uint32_t dimColor(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    r = (uint8_t)(r * 2 / 3);
    g = (uint8_t)(g * 2 / 3);
    b = (uint8_t)(b * 2 / 3);
    return (r << 16) | (g << 8) | b;
}

Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

// --- Atlas: row-based bin packing ---

Glyph Renderer::insertGlyph(int glyph_w, int glyph_h, const uint8_t* rgba_data) {
    Atlas& atlas = m_atlases[m_currentAtlas];

    if (atlas.row_extent + glyph_w > Atlas::SIZE ||
        atlas.row_baseline + glyph_h > Atlas::SIZE) {
        // Advance row first
        atlas.row_baseline += atlas.row_tallest;
        atlas.row_extent = 0;
        atlas.row_tallest = 0;

        if (atlas.row_baseline + glyph_h > Atlas::SIZE) {
            // Need a new atlas
            Atlas newAtlas;
            glGenTextures(1, &newAtlas.texture);
            glBindTexture(GL_TEXTURE_2D, newAtlas.texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Atlas::SIZE, Atlas::SIZE,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            m_atlases.push_back(newAtlas);
            m_currentAtlas = (int)m_atlases.size() - 1;
        }
    }

    Atlas& cur = m_atlases[m_currentAtlas];
    int x = cur.row_extent;
    int y = cur.row_baseline;

    glBindTexture(GL_TEXTURE_2D, cur.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, glyph_w, glyph_h,
                    GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);

    cur.row_extent += glyph_w;
    if (glyph_h > cur.row_tallest) cur.row_tallest = glyph_h;

    float s = (float)Atlas::SIZE;
    Glyph g;
    g.tex_id = cur.texture;
    g.u0 = (float)x / s;
    g.v0 = (float)y / s;
    g.u1 = (float)(x + glyph_w) / s;
    g.v1 = (float)(y + glyph_h) / s;
    g.width = glyph_w;
    g.height = glyph_h;
    return g;
}

// --- GDI scratchpad management ---

void Renderer::createGDIScratchpad(HDC hdc, HFONT hFont) {
    if (m_fallbackBitmap) { DeleteObject(m_fallbackBitmap); m_fallbackBitmap = nullptr; }
    if (m_fallbackMemDC) { DeleteDC(m_fallbackMemDC); m_fallbackMemDC = nullptr; }

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = Atlas::SIZE;
    bi.bmiHeader.biHeight = -Atlas::SIZE;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    m_fallbackBitmap = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &m_fallbackBits, nullptr, 0);
    if (!m_fallbackBitmap) return;

    m_fallbackMemDC = CreateCompatibleDC(hdc);
    m_fallbackOldBmp = SelectObject(m_fallbackMemDC, m_fallbackBitmap);
    m_fallbackOldFont = SelectObject(m_fallbackMemDC, hFont);

    RECT fullRc = {0, 0, Atlas::SIZE, Atlas::SIZE};
    HBRUSH bk = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(m_fallbackMemDC, &fullRc, bk);
    DeleteObject(bk);

    SetTextColor(m_fallbackMemDC, RGB(255, 255, 255));
    SetBkColor(m_fallbackMemDC, RGB(0, 0, 0));
    SetBkMode(m_fallbackMemDC, OPAQUE);
}

void Renderer::destroyGDIScratchpad() {
    if (m_fallbackOldBmp && m_fallbackMemDC) {
        SelectObject(m_fallbackMemDC, m_fallbackOldBmp);
    }
    if (m_fallbackOldFont && m_fallbackMemDC) {
        SelectObject(m_fallbackMemDC, m_fallbackOldFont);
    }
    if (m_fallbackBitmap) { DeleteObject(m_fallbackBitmap); m_fallbackBitmap = nullptr; }
    if (m_fallbackMemDC) { DeleteDC(m_fallbackMemDC); m_fallbackMemDC = nullptr; }
    m_fallbackOldBmp = nullptr;
    m_fallbackOldFont = nullptr;
    m_fallbackBits = nullptr;
}

// --- measureCellSize ---

void Renderer::measureCellSize() {
    HFONT hFont = nullptr;
    for (const wchar_t* name : FONT_NAMES) {
        hFont = CreateFontW(
            -m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH | FF_MODERN, name
        );
        if (!hFont) continue;

        HDC hdc = GetDC(m_hwnd);
        HGDIOBJ old = SelectObject(hdc, hFont);
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, L"X", 1, &sz);
        SelectObject(hdc, old);
        ReleaseDC(m_hwnd, hdc);

        if (sz.cx >= 4) {
            dbg("Font selected: %S (width=%d)\n", name, sz.cx);
            break;
        }
        DeleteObject(hFont);
        hFont = nullptr;
    }
    if (!hFont) {
        dbg("Using fallback Consolas\n");
        hFont = CreateFontW(-m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }

    HDC hdc = GetDC(m_hwnd);
    HGDIOBJ oldFontHdc = SelectObject(hdc, hFont);
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    SIZE charSz;
    GetTextExtentPoint32W(hdc, L"X", 1, &charSz);
    m_cellWidth = charSz.cx;
    m_cellHeight = tm.tmHeight;
    if (m_cellWidth < 1) m_cellWidth = 8;
    if (m_cellHeight < 1) m_cellHeight = 16;
    SelectObject(hdc, oldFontHdc);
    ReleaseDC(m_hwnd, hdc);
    DeleteObject(hFont);

    dbg("Cell size: %dx%d\n", m_cellWidth, m_cellHeight);
}

// --- getGlyph ---

Glyph* Renderer::getGlyph(wchar_t ch) {
    GlyphKey key = {ch, m_fontSize};
    auto it = m_glyphCache.find(key);
    if (it != m_glyphCache.end()) return &it->second;

    if (!m_fallbackMemDC || !m_fallbackBits) return nullptr;

    RECT cellRc = {0, 0, m_cellWidth, m_cellHeight};
    HBRUSH bk = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(m_fallbackMemDC, &cellRc, bk);
    DeleteObject(bk);

    TextOutW(m_fallbackMemDC, 0, 0, &ch, 1);

    SIZE csz = {};
    GetTextExtentPoint32W(m_fallbackMemDC, &ch, 1, &csz);

    uint32_t* src = (uint32_t*)m_fallbackBits;
    int gw = csz.cx;
    int gh = csz.cy;
    if (gw < 1) gw = 1;
    if (gh < 1) gh = 1;
    if (gw > m_cellWidth) gw = m_cellWidth;
    if (gh > m_cellHeight) gh = m_cellHeight;

    std::vector<uint8_t> rgba(gw * gh * 4);
    for (int y = 0; y < gh; y++) {
        for (int x = 0; x < gw; x++) {
            int srcIdx = y * Atlas::SIZE + x;
            uint32_t p = src[srcIdx];
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = p & 0xFF;
            uint8_t lum = (uint8_t)((r * 77 + g * 150 + b * 29 + 128) >> 8);
            rgba[(y * gw + x) * 4 + 0] = 255;
            rgba[(y * gw + x) * 4 + 1] = 255;
            rgba[(y * gw + x) * 4 + 2] = 255;
            rgba[(y * gw + x) * 4 + 3] = lum;
        }
    }

    Glyph g = insertGlyph(gw, gh, rgba.data());
    m_glyphCache[key] = g;
    dbg("Glyph loaded: U+%04X at atlas %d (%d,%d) size=%dx%d\n",
        ch, m_currentAtlas, (int)g.u0 * Atlas::SIZE, (int)g.v0 * Atlas::SIZE, gw, gh);
    return &m_glyphCache[key];
}

// --- loadCommonGlyphs ---

void Renderer::loadCommonGlyphs() {
    for (wchar_t c = 0x20; c < 0x7F; c++) {
        getGlyph(c);
    }
    dbg("Common glyphs loaded: %d\n", (int)m_glyphCache.size());
}

// --- clearGlyphCache ---

void Renderer::clearGlyphCache() {
    m_glyphCache.clear();
    for (auto& atlas : m_atlases) {
        atlas.row_extent = 0;
        atlas.row_baseline = 0;
        atlas.row_tallest = 0;
    }
    m_currentAtlas = 0;
}

// --- init ---

bool Renderer::init(HWND hwnd) {
    m_hwnd = hwnd;
    m_hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(m_hdc, &pfd);
    if (!pf) { dbg("ChoosePixelFormat failed\n"); return false; }
    SetPixelFormat(m_hdc, pf, &pfd);

    m_hglrc = wglCreateContext(m_hdc);
    if (!m_hglrc) { dbg("wglCreateContext failed\n"); return false; }
    wglMakeCurrent(m_hdc, m_hglrc);

    dbg("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));
    dbg("GL_RENDERER: %s\n", (const char*)glGetString(GL_RENDERER));
    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    dbg("GL_MAX_TEXTURE_SIZE: %d\n", maxTex);

    measureCellSize();

    // Create first atlas
    Atlas first;
    glGenTextures(1, &first.texture);
    glBindTexture(GL_TEXTURE_2D, first.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Atlas::SIZE, Atlas::SIZE,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    m_atlases.push_back(first);
    m_currentAtlas = 0;

    // Create GDI scratchpad
    HFONT hFont = CreateFontW(
        -m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH | FF_MODERN, FONT_NAMES[0]
    );
    if (!hFont) {
        hFont = CreateFontW(-m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }
    createGDIScratchpad(m_hdc, hFont);
    DeleteObject(hFont);

    loadCommonGlyphs();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_initialized = true;
    dbg("Renderer init OK, cell=%dx%d, fontSize=%d, atlases=%d, glyphs=%d\n",
        m_cellWidth, m_cellHeight, m_fontSize, (int)m_atlases.size(), (int)m_glyphCache.size());
    return true;
}

// --- setFontSize ---

void Renderer::setFontSize(int size) {
    if (size < 4 || size > 128) return;
    if (size == m_fontSize) return;

    dbg("Changing font size from %d to %d\n", m_fontSize, size);

    // Clear old GDI scratchpad
    destroyGDIScratchpad();

    m_fontSize = size;
    measureCellSize();
    clearGlyphCache();

    // Recreate GDI scratchpad for new cell dimensions
    HFONT hFont = CreateFontW(
        -m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH | FF_MODERN, FONT_NAMES[0]
    );
    if (!hFont) {
        hFont = CreateFontW(-m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }
    createGDIScratchpad(m_hdc, hFont);
    DeleteObject(hFont);

    loadCommonGlyphs();

    dbg("Font size changed to %d, new cell=%dx%d, glyphs=%d\n",
        m_fontSize, m_cellWidth, m_cellHeight, (int)m_glyphCache.size());
}

// --- shutdown ---

void Renderer::shutdown() {
    for (auto& atlas : m_atlases) {
        if (atlas.texture) { glDeleteTextures(1, &atlas.texture); atlas.texture = 0; }
    }
    m_atlases.clear();
    m_currentAtlas = 0;
    m_glyphCache.clear();

    destroyGDIScratchpad();

    if (m_hglrc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(m_hglrc); m_hglrc = nullptr; }
    if (m_hdc && m_hwnd) { ReleaseDC(m_hwnd, m_hdc); m_hdc = nullptr; }
    m_initialized = false;
}

// --- frame ---

void Renderer::beginFrame(int width, int height) {
    if (!m_initialized) return;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0.102f, 0.106f, 0.149f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_bgBatch.clear();
    m_glyphBatch.clear();
}

void Renderer::flushBatches() {
    if (!m_initialized) return;
    flushBgBatch();
    flushGlyphBatch();
}

void Renderer::present() {
    if (!m_initialized) return;
    SwapBuffers(m_hdc);
}

void Renderer::endFrame() {
    flushBatches();
    present();
}

// --- drawCell ---

void Renderer::drawCell(int x, int y, const Cell& cell, uint32_t bg) {
    float px = (float)(x * m_cellWidth);
    float py = (float)(y * m_cellHeight);
    float w = (float)m_cellWidth;
    float h = (float)m_cellHeight;

    if (cell.width == 0) return;

    m_bgBatch.push_back({px, py, w, h, bg});

    wchar_t ch = cell.ch;
    if (ch == L' ' || ch == 0) return;

    uint32_t fg = cell.fg;
    if (cell.dim) fg = dimColor(fg);

    GlyphKey key = {ch, m_fontSize};
    auto it = m_glyphCache.find(key);
    Glyph* gi = (it != m_glyphCache.end())
        ? &it->second
        : getGlyph(ch);
    if (!gi) return;

    bool boxDraw = isBoxDrawing(static_cast<uint32_t>(ch));
    float cx, cy;
    if (boxDraw) {
        cx = px;
        cy = py;
    } else {
        cx = px + (w - gi->width) / 2.0f;
        cy = py + (h - gi->height) / 2.0f;
    }

    m_glyphBatch.push_back({cx, cy, (float)gi->width, (float)gi->height,
                            gi->u0, gi->v0, gi->u1, gi->v1, fg, gi->tex_id});

    if (cell.bold) {
        m_glyphBatch.push_back({cx + 1.0f, cy, (float)gi->width, (float)gi->height,
                                gi->u0, gi->v0, gi->u1, gi->v1, fg, gi->tex_id});
    }

    for (int i = 0; i < cell.combinedCount; i++) {
        wchar_t comb = cell.combined[i];
        GlyphKey ckey = {comb, m_fontSize};
        auto cit = m_glyphCache.find(ckey);
        Glyph* cgi = (cit != m_glyphCache.end()) ? &cit->second : getGlyph(comb);
        if (!cgi) continue;
        float ccx = cx + (gi->width - cgi->width) / 2.0f;
        float ccy = cy + (gi->height - cgi->height) / 2.0f;
        m_glyphBatch.push_back({ccx, ccy, (float)cgi->width, (float)cgi->height,
                                cgi->u0, cgi->v0, cgi->u1, cgi->v1, fg, cgi->tex_id});
    }
}

// --- flush ---

void Renderer::flushBgBatch() {
    if (m_bgBatch.empty()) return;
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (const auto& q : m_bgBatch) {
        glColor4ub((q.color >> 16) & 0xFF, (q.color >> 8) & 0xFF, q.color & 0xFF, 255);
        glVertex2f(q.x, q.y);
        glVertex2f(q.x + q.w, q.y);
        glVertex2f(q.x + q.w, q.y + q.h);
        glVertex2f(q.x, q.y + q.h);
    }
    glEnd();
}

void Renderer::flushGlyphBatch() {
    if (m_glyphBatch.empty()) return;

    GLuint lastTex = 0;
    bool inQuad = false;
    for (const auto& q : m_glyphBatch) {
        if (q.tex_id != lastTex) {
            if (inQuad) {
                glEnd();
                inQuad = false;
            }
            glBindTexture(GL_TEXTURE_2D, q.tex_id);
            glEnable(GL_TEXTURE_2D);
            glBegin(GL_QUADS);
            inQuad = true;
            lastTex = q.tex_id;
        }
        glColor4ub((q.color >> 16) & 0xFF, (q.color >> 8) & 0xFF, q.color & 0xFF, 255);
        glTexCoord2f(q.u0, q.v0); glVertex2f(q.x, q.y);
        glTexCoord2f(q.u1, q.v0); glVertex2f(q.x + q.w, q.y);
        glTexCoord2f(q.u1, q.v1); glVertex2f(q.x + q.w, q.y + q.h);
        glTexCoord2f(q.u0, q.v1); glVertex2f(q.x, q.y + q.h);
    }
    if (inQuad) glEnd();
}

// --- drawCursor ---

void Renderer::drawCursor(int x, int y, int cellW, int cellH, uint32_t color) {
    float px = (float)(x * cellW);
    float py = (float)(y * cellH);
    float w = (float)cellW;
    float h = (float)cellH;

    glDisable(GL_TEXTURE_2D);
    glColor4ub((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);
    glBegin(GL_QUADS);
        glVertex2f(px, py);
        glVertex2f(px + w, py);
        glVertex2f(px + w, py + h);
        glVertex2f(px, py + h);
    glEnd();
}
