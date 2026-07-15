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

static const wchar_t* FALLBACK_FONT_NAMES[] = {
    L"MS Gothic",
    L"MS Mincho",
    L"MingLiU",
    L"NSimSun",
    L"Arial Unicode MS",
    L"Segoe UI Symbol",
    L"Segoe UI Emoji",
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

    if (!createFontAtlas(m_hdc)) {
        dbg("createFontAtlas FAILED\n");
        return false;
    }

    createFallbackAtlas(m_hdc);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_initialized = true;
    dbg("Renderer init OK, cell=%dx%d, fontSize=%d\n", m_cellWidth, m_cellHeight, m_fontSize);
    return true;
}

void Renderer::setFontSize(int size) {
    if (size < 4 || size > 128) return; // Reasonable limits
    if (size == m_fontSize) return;

    dbg("Changing font size from %d to %d\n", m_fontSize, size);
    
    // Clean up existing font atlas
    if (m_fontTexture) {
        glDeleteTextures(1, &m_fontTexture);
        m_fontTexture = 0;
    }
    if (m_fallbackTexture) {
        glDeleteTextures(1, &m_fallbackTexture);
        m_fallbackTexture = 0;
    }
    if (m_fallbackBitmap) { DeleteObject(m_fallbackBitmap); m_fallbackBitmap = nullptr; }
    if (m_fallbackMemDC) { DeleteDC(m_fallbackMemDC); m_fallbackMemDC = nullptr; }

    m_glyphs.clear();
    m_fallbackGlyphs.clear();
    m_hasFallback = false;
    m_fontSize = size;

    // Recreate font atlas with new size
    createFontAtlas(m_hdc);
    createFallbackAtlas(m_hdc);

    dbg("Font size changed to %d, new cell=%dx%d\n", m_fontSize, m_cellWidth, m_cellHeight);
}

void Renderer::shutdown() {
    if (m_fontTexture) { glDeleteTextures(1, &m_fontTexture); m_fontTexture = 0; }
    if (m_fallbackTexture) { glDeleteTextures(1, &m_fallbackTexture); m_fallbackTexture = 0; }
    if (m_fallbackBitmap) { DeleteObject(m_fallbackBitmap); m_fallbackBitmap = nullptr; }
    if (m_fallbackMemDC) { DeleteDC(m_fallbackMemDC); m_fallbackMemDC = nullptr; }
    if (m_hglrc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(m_hglrc); m_hglrc = nullptr; }
    if (m_hdc && m_hwnd) { ReleaseDC(m_hwnd, m_hdc); m_hdc = nullptr; }
    m_initialized = false;
}

bool Renderer::createFontAtlas(HDC hdc) {
    SetMapMode(hdc, MM_TEXT);

    HFONT hFont = nullptr;
    for (const wchar_t* name : FONT_NAMES) {
        hFont = CreateFontW(
            -m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH | FF_MODERN, name
        );
        if (!hFont) continue;

        HGDIOBJ old = SelectObject(hdc, hFont);
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, L"X", 1, &sz);
        SelectObject(hdc, old);

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

    dbg("Cell size: %dx%d\n", m_cellWidth, m_cellHeight);

    std::vector<wchar_t> chars;
    for (wchar_t c = 0x20; c < 0x80; c++) chars.push_back(c);
    for (wchar_t c = 0xA0; c < 0x250; c++) chars.push_back(c);
    for (wchar_t c = 0x300; c < 0x530; c++) chars.push_back(c);
    for (wchar_t c = 0xE000; c < 0xF000; c++) chars.push_back(c);
    for (wchar_t c = 0x2500; c < 0x2580; c++) chars.push_back(c);
    for (wchar_t c = 0x2590; c < 0x2600; c++) chars.push_back(c);
    for (wchar_t c = 0x2640; c < 0x2670; c++) chars.push_back(c);
    for (wchar_t c = 0x2700; c < 0x27C0; c++) chars.push_back(c);
    for (wchar_t c = 0x27C0; c < 0x2800; c++) chars.push_back(c);
    for (wchar_t c = 0x2800; c < 0x2900; c++) chars.push_back(c);
    for (wchar_t c = 0x2900; c < 0x2C00; c++) chars.push_back(c);
    for (wchar_t c = 0x2C00; c < 0x2E80; c++) chars.push_back(c);
    for (wchar_t c = 0x2000; c < 0x2070; c++) chars.push_back(c);
    for (wchar_t c = 0x2100; c < 0x2150; c++) chars.push_back(c);
    for (wchar_t c = 0x2190; c < 0x2200; c++) chars.push_back(c);
    for (wchar_t c = 0x2200; c < 0x2300; c++) chars.push_back(c);
    for (wchar_t c = 0x2300; c < 0x2400; c++) chars.push_back(c);
    for (wchar_t c = 0x2460; c < 0x2500; c++) chars.push_back(c);
    for (wchar_t c = 0x2580; c < 0x25A0; c++) chars.push_back(c);
    for (wchar_t c = 0x25A0; c < 0x2600; c++) chars.push_back(c);
    for (wchar_t c = 0x3000; c < 0x3040; c++) chars.push_back(c);
    for (wchar_t c = 0x3041; c < 0x3097; c++) chars.push_back(c);
    for (wchar_t c = 0x30A1; c < 0x3100; c++) chars.push_back(c);
    for (wchar_t c = 0x3100; c < 0x3130; c++) chars.push_back(c);
    for (wchar_t c = 0x3200; c < 0x3300; c++) chars.push_back(c);
    for (wchar_t c = 0xFF01; c < 0xFF61; c++) chars.push_back(c);
    for (wchar_t c = 0xFFE0; c < 0xFFE7; c++) chars.push_back(c);
    for (wchar_t c = 0xFE30; c < 0xFE50; c++) chars.push_back(c);

    int totalChars = (int)chars.size();
    int rows = (totalChars + ATLAS_COLS - 1) / ATLAS_COLS;

    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    int maxDim = maxTex > 0 ? maxTex : 4096;

    m_atlasTexW = ATLAS_COLS * m_cellWidth;
    m_atlasTexH = rows * m_cellHeight;

    auto nextPow2 = [](int v) { int p = 1; while (p < v) p <<= 1; return p; };
    m_atlasTexW = nextPow2(m_atlasTexW);
    m_atlasTexH = nextPow2(m_atlasTexH);

    if (m_atlasTexW > maxDim) m_atlasTexW = maxDim;
    if (m_atlasTexH > maxDim) m_atlasTexH = maxDim;

    dbg("Atlas: %d chars, grid=%dx%d, tex=%dx%d (max=%d)\n",
        totalChars, ATLAS_COLS, rows, m_atlasTexW, m_atlasTexH, maxDim);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = m_atlasTexW;
    bi.bmiHeader.biHeight = -m_atlasTexH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBitmap) { dbg("CreateDIBSection failed\n"); return false; }

    HDC memDC = CreateCompatibleDC(hdc);
    HGDIOBJ oldBmp = SelectObject(memDC, hBitmap);
    HGDIOBJ oldFont = SelectObject(memDC, hFont);

    RECT fullRc = {0, 0, m_atlasTexW, m_atlasTexH};
    HBRUSH bk = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(memDC, &fullRc, bk);
    DeleteObject(bk);

    SetTextColor(memDC, RGB(255, 255, 255));
    SetBkColor(memDC, RGB(0, 0, 0));
    SetBkMode(memDC, OPAQUE);

    int maxRow = 0;
    for (int i = 0; i < totalChars; i++) {
        wchar_t ch = chars[i];
        int col = i % ATLAS_COLS;
        int row = i / ATLAS_COLS;
        if (row >= m_atlasTexH / m_cellHeight) break;
        int px = col * m_cellWidth;
        int py = row * m_cellHeight;

        TextOutW(memDC, px, py, &ch, 1);

        SIZE csz = {};
        GetTextExtentPoint32W(memDC, &ch, 1, &csz);

        GlyphInfo gi = {};
        gi.u0 = (float)px / m_atlasTexW;
        gi.v0 = (float)py / m_atlasTexH;
        gi.u1 = (float)(px + csz.cx) / m_atlasTexW;
        gi.v1 = (float)(py + csz.cy) / m_atlasTexH;
        gi.width = csz.cx;
        gi.height = csz.cy;
        m_glyphs[ch] = gi;
        if (row > maxRow) maxRow = row;
    }

    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);

    uint32_t* src = (uint32_t*)bits;
    int nonZero = 0;
    std::vector<uint8_t> rgba(m_atlasTexW * m_atlasTexH * 4);
    for (int i = 0; i < m_atlasTexW * m_atlasTexH; i++) {
        uint32_t px = src[i];
        uint8_t r = (px >> 16) & 0xFF;
        uint8_t g = (px >> 8) & 0xFF;
        uint8_t b = px & 0xFF;
        uint8_t lum = (uint8_t)((r * 77 + g * 150 + b * 29 + 128) >> 8);
        if (lum > 0) nonZero++;
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = lum;
    }

    dbg("Font bitmap: %d non-zero pixels out of %d\n", nonZero, m_atlasTexW * m_atlasTexH);

    DeleteObject(hBitmap);
    DeleteObject(hFont);

    glGenTextures(1, &m_fontTexture);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_atlasTexW, m_atlasTexH,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    GLenum err = glGetError();
    if (err) dbg("glTexImage2D error: 0x%X\n", err);

    dbg("Font atlas created OK, texture=%d, glyphs=%d\n", m_fontTexture, (int)m_glyphs.size());
    return true;
}

bool Renderer::createFallbackAtlas(HDC hdc) {
    HFONT hFont = nullptr;
    for (const wchar_t* name : FALLBACK_FONT_NAMES) {
        hFont = CreateFontW(
            -m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH | FF_MODERN, name
        );
        if (!hFont) continue;

        HGDIOBJ old = SelectObject(hdc, hFont);
        wchar_t testChar = 0x4E2D;
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, &testChar, 1, &sz);
        SelectObject(hdc, old);

        if (sz.cx >= 4) {
            dbg("Fallback font found: %S\n", name);
            break;
        }
        DeleteObject(hFont);
        hFont = nullptr;
    }
    if (!hFont) {
        dbg("No fallback font found\n");
        return false;
    }

    int fbTexW = ATLAS_COLS * m_cellWidth;
    int fbTexH = 8 * m_cellHeight;

    auto nextPow2 = [](int v) { int p = 1; while (p < v) p <<= 1; return p; };
    m_fallbackTexW = nextPow2(fbTexW);
    m_fallbackTexH = nextPow2(fbTexH);

    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    if (m_fallbackTexW > maxTex) m_fallbackTexW = maxTex;
    if (m_fallbackTexH > maxTex) m_fallbackTexH = maxTex;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = m_fallbackTexW;
    bi.bmiHeader.biHeight = -m_fallbackTexH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    m_fallbackBitmap = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &m_fallbackBits, nullptr, 0);
    if (!m_fallbackBitmap) { DeleteObject(hFont); return false; }

    m_fallbackMemDC = CreateCompatibleDC(hdc);
    m_fallbackOldBmp = SelectObject(m_fallbackMemDC, m_fallbackBitmap);
    m_fallbackOldFont = SelectObject(m_fallbackMemDC, hFont);

    RECT fullRc = {0, 0, m_fallbackTexW, m_fallbackTexH};
    HBRUSH bk = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(m_fallbackMemDC, &fullRc, bk);
    DeleteObject(bk);

    SetTextColor(m_fallbackMemDC, RGB(255, 255, 255));
    SetBkColor(m_fallbackMemDC, RGB(0, 0, 0));
    SetBkMode(m_fallbackMemDC, OPAQUE);

    glGenTextures(1, &m_fallbackTexture);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fallbackTexW, m_fallbackTexH,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    m_hasFallback = true;
    m_fallbackAtlasRow = 0;

    dbg("Fallback atlas created, texture=%d, tex=%dx%d\n",
        m_fallbackTexture, m_fallbackTexW, m_fallbackTexH);
    return true;
}

void Renderer::addGlyphToAtlas(HDC hdc, HFONT hFont, wchar_t ch) {
    if (!m_hasFallback) return;
    if (m_fallbackGlyphs.find(ch) != m_fallbackGlyphs.end()) return;

    HGDIOBJ oldFont = SelectObject(m_fallbackMemDC, hFont);

    int col = (int)m_fallbackGlyphs.size() % ATLAS_COLS;
    int row = m_fallbackAtlasRow;
    if (col == 0 && !m_fallbackGlyphs.empty()) {
        row++;
        m_fallbackAtlasRow = row;
    }

    if (row * m_cellHeight >= m_fallbackTexH) {
        SelectObject(m_fallbackMemDC, oldFont);
        return;
    }

    int px = col * m_cellWidth;
    int py = row * m_cellHeight;

    RECT cellRc = {px, py, px + m_cellWidth, py + m_cellHeight};
    HBRUSH bk = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(m_fallbackMemDC, &cellRc, bk);
    DeleteObject(bk);

    TextOutW(m_fallbackMemDC, px, py, &ch, 1);

    SIZE csz = {};
    GetTextExtentPoint32W(m_fallbackMemDC, &ch, 1, &csz);

    GlyphInfo gi = {};
    gi.u0 = (float)px / m_fallbackTexW;
    gi.v0 = (float)py / m_fallbackTexH;
    gi.u1 = (float)(px + csz.cx) / m_fallbackTexW;
    gi.v1 = (float)(py + csz.cy) / m_fallbackTexH;
    gi.width = csz.cx;
    gi.height = csz.cy;
    m_fallbackGlyphs[ch] = gi;

    SelectObject(m_fallbackMemDC, oldFont);

    uint32_t* src = (uint32_t*)m_fallbackBits;
    std::vector<uint8_t> sub(m_cellWidth * m_cellHeight * 4);
    for (int y = 0; y < m_cellHeight; y++) {
        for (int x = 0; x < m_cellWidth; x++) {
            int srcIdx = (py + y) * m_fallbackTexW + (px + x);
            int dstIdx = y * m_cellWidth + x;
            uint32_t px2 = src[srcIdx];
            uint8_t r = (px2 >> 16) & 0xFF;
            uint8_t g = (px2 >> 8) & 0xFF;
            uint8_t b = px2 & 0xFF;
            uint8_t lum = (uint8_t)((r * 77 + g * 150 + b * 29 + 128) >> 8);
            sub[dstIdx * 4 + 0] = 255;
            sub[dstIdx * 4 + 1] = 255;
            sub[dstIdx * 4 + 2] = 255;
            sub[dstIdx * 4 + 3] = lum;
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, px, py, m_cellWidth, m_cellHeight,
                    GL_RGBA, GL_UNSIGNED_BYTE, sub.data());

    dbg("Fallback glyph added: U+%04X at (%d,%d)\n", ch, col, row);
}

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

    auto findGlyph = [&](wchar_t c) -> const GlyphInfo* {
        auto it = m_glyphs.find(c);
        if (it != m_glyphs.end()) return &it->second;
        if (m_hasFallback) {
            auto fb = m_fallbackGlyphs.find(c);
            if (fb != m_fallbackGlyphs.end()) return &fb->second;
            HDC hdc = m_hdc;
            HFONT hFont = (HFONT)GetCurrentObject(m_fallbackMemDC, OBJ_FONT);
            if (hFont) {
                const_cast<Renderer*>(this)->addGlyphToAtlas(hdc, hFont, c);
                fb = m_fallbackGlyphs.find(c);
                if (fb != m_fallbackGlyphs.end()) return &fb->second;
            }
        }
        return nullptr;
    };

    const GlyphInfo* gi = findGlyph(ch);
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
                            gi->u0, gi->v0, gi->u1, gi->v1, fg});

    if (cell.bold) {
        m_glyphBatch.push_back({cx + 1.0f, cy, (float)gi->width, (float)gi->height,
                                gi->u0, gi->v0, gi->u1, gi->v1, fg});
    }

    for (int i = 0; i < cell.combinedCount; i++) {
        wchar_t comb = cell.combined[i];
        const GlyphInfo* cgi = findGlyph(comb);
        if (!cgi) continue;
        float ccx = cx + (gi->width - cgi->width) / 2.0f;
        float ccy = cy + (gi->height - cgi->height) / 2.0f;
        m_glyphBatch.push_back({ccx, ccy, (float)cgi->width, (float)cgi->height,
                                cgi->u0, cgi->v0, cgi->u1, cgi->v1, fg});
    }
}

void Renderer::renderCombinedGlyphs(HDC hdc, HFONT hFont, int x, int y, const Cell& cell, uint32_t fg) {
    (void)hdc; (void)hFont; (void)x; (void)y; (void)cell; (void)fg;
}

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

    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (const auto& q : m_glyphBatch) {
        glColor4ub((q.color >> 16) & 0xFF, (q.color >> 8) & 0xFF, q.color & 0xFF, 255);
        glTexCoord2f(q.u0, q.v0); glVertex2f(q.x, q.y);
        glTexCoord2f(q.u1, q.v0); glVertex2f(q.x + q.w, q.y);
        glTexCoord2f(q.u1, q.v1); glVertex2f(q.x + q.w, q.y + q.h);
        glTexCoord2f(q.u0, q.v1); glVertex2f(q.x, q.y + q.h);
    }
    glEnd();
}

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
