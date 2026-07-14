#include "renderer.h"
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_initialized = true;
    dbg("Renderer init OK, cell=%dx%d\n", m_cellWidth, m_cellHeight);
    return true;
}

void Renderer::shutdown() {
    if (m_fontTexture) { glDeleteTextures(1, &m_fontTexture); m_fontTexture = 0; }
    if (m_hglrc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(m_hglrc); m_hglrc = nullptr; }
    if (m_hdc && m_hwnd) { ReleaseDC(m_hwnd, m_hdc); m_hdc = nullptr; }
    m_initialized = false;
}

bool Renderer::createFontAtlas(HDC hdc) {
    SetMapMode(hdc, MM_TEXT);

    HFONT hFont = nullptr;
    for (const wchar_t* name : FONT_NAMES) {
        hFont = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, name
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
        hFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
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
    for (wchar_t c = 0xE000; c < 0xF000; c++) chars.push_back(c);
    for (wchar_t c = 0x2500; c < 0x2580; c++) chars.push_back(c);
    for (wchar_t c = 0x2590; c < 0x2600; c++) chars.push_back(c);
    for (wchar_t c = 0x2640; c < 0x2670; c++) chars.push_back(c);
    for (wchar_t c = 0x2700; c < 0x27C0; c++) chars.push_back(c);
    for (wchar_t c = 0x27C0; c < 0x2800; c++) chars.push_back(c);
    for (wchar_t c = 0x2800; c < 0x2900; c++) chars.push_back(c);
    for (wchar_t c = 0x2900; c < 0x2C00; c++) chars.push_back(c);
    for (wchar_t c = 0x2C00; c < 0x2E80; c++) chars.push_back(c);

    int totalChars = (int)chars.size();
    int rows = (totalChars + ATLAS_COLS - 1) / ATLAS_COLS;

    m_atlasTexW = ATLAS_COLS * m_cellWidth;
    m_atlasTexH = rows * m_cellHeight;

    auto nextPow2 = [](int v) { int p = 1; while (p < v) p <<= 1; return p; };
    m_atlasTexW = nextPow2(m_atlasTexW);
    m_atlasTexH = nextPow2(m_atlasTexH);

    dbg("Atlas: %d chars, grid=%dx%d, tex=%dx%d\n", totalChars, ATLAS_COLS, rows, m_atlasTexW, m_atlasTexH);

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

    for (int i = 0; i < totalChars; i++) {
        wchar_t ch = chars[i];
        int col = i % ATLAS_COLS;
        int row = i / ATLAS_COLS;
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
        uint8_t lum = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
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

void Renderer::drawCell(int x, int y, wchar_t ch, uint32_t fg, uint32_t bg, bool /*bold*/) {
    float px = (float)(x * m_cellWidth);
    float py = (float)(y * m_cellHeight);
    float w = (float)m_cellWidth;
    float h = (float)m_cellHeight;

    m_bgBatch.push_back({px, py, w, h, bg});

    if (ch == L' ' || ch == 0) return;

    auto it = m_glyphs.find(ch);
    if (it == m_glyphs.end()) return;
    const GlyphInfo& gi = it->second;

    float cx = px + (w - gi.width) / 2.0f;
    float cy = py + (h - gi.height) / 2.0f;

    m_glyphBatch.push_back({cx, cy, (float)gi.width, (float)gi.height,
                            gi.u0, gi.v0, gi.u1, gi.v1, fg});
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
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
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
