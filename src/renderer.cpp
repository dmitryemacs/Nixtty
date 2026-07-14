#define GL_SILENCE_DEPRECATION
#include "renderer.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

static const char* FONT_NAMES[] = {
    "Hack Nerd Font Mono",
    "Hack Nerd Font",
    "Menlo",
    "Monaco",
    "Courier New",
};

static FILE* r_log = nullptr;
void renderer_set_log_file(FILE* f) { r_log = f; }

static void dbg(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (r_log) { fprintf(r_log, "[RND] %s", buf); fflush(r_log); }
}

Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

bool Renderer::init(GLFWwindow* window) {
    m_window = window;

    dbg("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));
    dbg("GL_RENDERER: %s\n", (const char*)glGetString(GL_RENDERER));
    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    dbg("GL_MAX_TEXTURE_SIZE: %d\n", maxTex);

    int fbW, fbH, winW, winH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glfwGetWindowSize(window, &winW, &winH);

    if (!createFontAtlas(fbW, fbH, winW, winH)) {
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
    m_initialized = false;
}

bool Renderer::createFontAtlas(int fbW, int fbH, int winW, int winH) {
    float scaleFactor = (float)fbW / (float)winW;
    if (scaleFactor < 1.0f) scaleFactor = 1.0f;
    dbg("Scale factor: %.1f\n", scaleFactor);

    const double fontSize = 16.0;
    const double atlasFontSize = fontSize * scaleFactor;
    CTFontRef ctFont = nullptr;

    for (const char* name : FONT_NAMES) {
        CFStringRef fontName = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
        ctFont = CTFontCreateWithName(fontName, atlasFontSize, nullptr);
        CFRelease(fontName);

        if (ctFont) {
            UniChar xc = 'X';
            CGGlyph g = 0;
            CTFontGetGlyphsForCharacters(ctFont, &xc, &g, 1);
            CGFloat w = CTFontGetAdvancesForGlyphs(ctFont, kCTFontOrientationDefault, &g, nullptr, 1);
            if (w >= 4.0 * scaleFactor) {
                dbg("Font selected: %s (advancement=%.1f @%.0fpt)\n", name, w, atlasFontSize);
                break;
            }
            CFRelease(ctFont);
            ctFont = nullptr;
        }
    }

    if (!ctFont) {
        dbg("Using fallback Menlo\n");
        CFStringRef fontName = CFStringCreateWithCString(kCFAllocatorDefault, "Menlo", kCFStringEncodingUTF8);
        ctFont = CTFontCreateWithName(fontName, atlasFontSize, nullptr);
        CFRelease(fontName);
    }
    if (!ctFont) {
        dbg("No font found\n");
        return false;
    }

    UniChar xc = 'X';
    CGGlyph glyphX = 0;
    CTFontGetGlyphsForCharacters(ctFont, &xc, &glyphX, 1);
    double advance = CTFontGetAdvancesForGlyphs(ctFont, kCTFontOrientationDefault, &glyphX, nullptr, 1);
    double ascent = CTFontGetAscent(ctFont);
    double descent = CTFontGetDescent(ctFont);
    double leading = CTFontGetLeading(ctFont);

    int atlasCellW = (int)ceil(advance) + 2;
    int atlasCellH = (int)ceil(ascent + descent + leading) + 2;
    if (atlasCellW < 2) atlasCellW = 16;
    if (atlasCellH < 2) atlasCellH = 32;

    m_cellWidth = (int)ceil((double)atlasCellW / scaleFactor);
    m_cellHeight = (int)ceil((double)atlasCellH / scaleFactor);

    dbg("Atlas cell: %dx%d, logical cell: %dx%d (ascent=%.1f descent=%.1f leading=%.1f advance=%.1f)\n",
        atlasCellW, atlasCellH, m_cellWidth, m_cellHeight, ascent, descent, leading, advance);

    std::vector<wchar_t> chars;
    for (wchar_t c = 0x20; c < 0x80; c++) chars.push_back(c);
    for (wchar_t c = 0xA0; c < 0x250; c++) chars.push_back(c);
    for (wchar_t c = 0x370; c < 0x400; c++) chars.push_back(c);
    for (wchar_t c = 0x400; c < 0x530; c++) chars.push_back(c);
    for (wchar_t c = 0x2190; c < 0x2200; c++) chars.push_back(c);
    for (wchar_t c = 0x2200; c < 0x2300; c++) chars.push_back(c);
    for (wchar_t c = 0x2500; c < 0x2580; c++) chars.push_back(c);
    for (wchar_t c = 0x2580; c < 0x25A0; c++) chars.push_back(c);
    for (wchar_t c = 0x25A0; c < 0x2600; c++) chars.push_back(c);
    for (wchar_t c = 0x2600; c < 0x2700; c++) chars.push_back(c);
    for (wchar_t c = 0x2700; c < 0x2800; c++) chars.push_back(c);
    for (wchar_t c = 0x2800; c < 0x2900; c++) chars.push_back(c);
    for (wchar_t c = 0x3000; c < 0x3040; c++) chars.push_back(c);
    for (wchar_t c = 0x30A0; c < 0x3100; c++) chars.push_back(c);
    for (wchar_t c = 0x3200; c < 0x3300; c++) chars.push_back(c);
    for (wchar_t c = 0xE0A0; c <= 0xE0D4; c++) chars.push_back(c);
    for (wchar_t c = 0xE700; c <= 0xE7C5; c++) chars.push_back(c);
    for (wchar_t c = 0xE300; c <= 0xE3E3; c++) chars.push_back(c);
    for (wchar_t c = 0xE5FA; c <= 0xE6AC; c++) chars.push_back(c);
    for (wchar_t c = 0xE000; c <= 0xE100; c++) chars.push_back(c);
    for (wchar_t c = 0xF000; c <= 0xF2FF; c++) chars.push_back(c);
    for (wchar_t c = 0xF400; c <= 0xF532; c++) chars.push_back(c);
    for (wchar_t c = 0xED00; c <= 0xEF8C; c++) chars.push_back(c);
    for (wchar_t c = 0xFF00; c <= 0xFFEF; c++) chars.push_back(c);
    for (wchar_t c = 0x2590; c <= 0x259F; c++) chars.push_back(c);

    std::sort(chars.begin(), chars.end());
    chars.erase(std::unique(chars.begin(), chars.end()), chars.end());

    int totalChars = (int)chars.size();
    int rows = (totalChars + ATLAS_COLS - 1) / ATLAS_COLS;

    m_atlasTexW = ATLAS_COLS * atlasCellW;
    m_atlasTexH = rows * atlasCellH;

    auto nextPow2 = [](int v) { int p = 1; while (p < v) p <<= 1; return p; };
    m_atlasTexW = nextPow2(m_atlasTexW);
    m_atlasTexH = nextPow2(m_atlasTexH);

    dbg("Atlas: %d chars, grid=%dx%d, tex=%dx%d\n", totalChars, ATLAS_COLS, rows, m_atlasTexW, m_atlasTexH);

    std::vector<uint8_t> rgba(m_atlasTexW * m_atlasTexH * 4, 0);

    for (int i = 0; i < totalChars; i++) {
        wchar_t ch = chars[i];
        int col = i % ATLAS_COLS;
        int row = i / ATLAS_COLS;
        int px = col * atlasCellW;
        int py = row * atlasCellH;

        UniChar uc = (UniChar)ch;
        CGGlyph glyph = 0;
        CTFontGetGlyphsForCharacters(ctFont, &uc, &glyph, 1);
        if (glyph == 0) continue;

        int gw = atlasCellW;
        int gh = atlasCellH;

        size_t bitsPerComponent = 8;
        size_t bytesPerRow = gw * 4;
        std::vector<uint8_t> glyphBitmap(gw * gh * 4, 0);

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            glyphBitmap.data(), gw, gh, bitsPerComponent, bytesPerRow,
            colorSpace, kCGImageAlphaPremultipliedLast);
        CGColorSpaceRelease(colorSpace);

        if (ctx) {
            CGContextSetRGBFillColor(ctx, 1.0, 1.0, 1.0, 1.0);
            CGContextSetTextDrawingMode(ctx, kCGTextFill);

            double adv = CTFontGetAdvancesForGlyphs(ctFont, kCTFontOrientationDefault, &glyph, nullptr, 1);
            CGFloat gx = (CGFloat)((gw - adv) / 2.0);
            CGFloat gy = (CGFloat)((gh - ascent + descent) / 2.0);
            CGPoint point = { gx, gy };
            CTFontDrawGlyphs(ctFont, &glyph, &point, 1, ctx);

            CGContextRelease(ctx);
        }

        for (int srcY = 0; srcY < gh && (py + srcY) < m_atlasTexH; srcY++) {
            for (int srcX = 0; srcX < gw && (px + srcX) < m_atlasTexW; srcX++) {
                int srcIdx = (srcY * gw + srcX) * 4;
                int dstIdx = ((py + srcY) * m_atlasTexW + (px + srcX)) * 4;
                uint8_t r = glyphBitmap[srcIdx + 0];
                uint8_t g = glyphBitmap[srcIdx + 1];
                uint8_t b = glyphBitmap[srcIdx + 2];
                uint8_t a = glyphBitmap[srcIdx + 3];
                uint8_t lum = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
                if (a > lum) lum = a;
                rgba[dstIdx + 0] = 255;
                rgba[dstIdx + 1] = 255;
                rgba[dstIdx + 2] = 255;
                rgba[dstIdx + 3] = lum;
            }
        }

        GlyphInfo gi = {};
        gi.u0 = (float)px / m_atlasTexW;
        gi.v0 = (float)py / m_atlasTexH;
        gi.u1 = (float)(px + atlasCellW) / m_atlasTexW;
        gi.v1 = (float)(py + atlasCellH) / m_atlasTexH;
        gi.width = m_cellWidth;
        gi.height = m_cellHeight;
        m_glyphs[ch] = gi;
    }

    CFRelease(ctFont);

    dbg("Font atlas created, glyphs=%d\n", (int)m_glyphs.size());

    glGenTextures(1, &m_fontTexture);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_atlasTexW, m_atlasTexH,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    GLenum err = glGetError();
    if (err) dbg("glTexImage2D error: 0x%X\n", err);

    return true;
}

void Renderer::beginFrame(int fbWidth, int fbHeight, int winWidth, int winHeight) {
    if (!m_initialized) return;

    glViewport(0, 0, fbWidth, fbHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, winWidth, winHeight, 0, -1, 1);
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
    if (!m_initialized || !m_window) return;
    glfwSwapBuffers(m_window);
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

    // Glyph is already cell-sized and centered in atlas, draw at cell position
    m_glyphBatch.push_back({px, py, w, h,
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
