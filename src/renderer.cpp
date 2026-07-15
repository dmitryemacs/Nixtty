#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

// ─── CF/CG smart pointers ─────────────────────────────────────────────────────

// RAII wrapper for CFTypeRef (CTFont, CFString, CFDictionary, CFAttributedString, CTLine, etc.)
struct CFReleaser {
    CFTypeRef ref;
    CFReleaser(CFTypeRef r = nullptr) : ref(r) {}
    ~CFReleaser() { if (ref) CFRelease(ref); }
    CFReleaser(const CFReleaser&) = delete;
    CFReleaser& operator=(const CFReleaser&) = delete;
    CFReleaser(CFReleaser&& other) noexcept : ref(other.ref) { other.ref = nullptr; }
    CFReleaser& operator=(CFReleaser&& other) noexcept { 
        if (this != &other) { 
            if (ref) CFRelease(ref); 
            ref = other.ref; 
            other.ref = nullptr; 
        } 
        return *this;
    }
    operator CFTypeRef() const { return ref; }
    CFTypeRef get() const { return ref; }
};

// ─── Shaders ────────────────────────────────────────────────────────────────

static const char* TEXT_VS = R"(
#version 330 core
layout(location=0) in vec2 aQuadVertex;
layout(location=1) in ivec2 aCellPos;
layout(location=2) in ivec4 aGlyphRect;
layout(location=3) in vec4 aUVRect;
layout(location=4) in vec4 aFgColor;
layout(location=5) in vec4 aBgColor;
layout(location=6) in int aColored;

uniform vec4 projection;  // (offsetX, offsetY, screenW, screenH) in fb pixels
uniform vec2 cellSize;    // cell pixel size in fb pixels
uniform int renderingPass;

out vec2 TexCoords;
out vec4 TextColor;
out vec4 BgColor;
flat out int Colored;

void main() {
    vec2 cellPos = vec2(aCellPos) * cellSize;

    if (renderingPass == 0) {
        vec2 pos = cellPos + aQuadVertex * cellSize;
        gl_Position = vec4(
            (pos.x + projection.x) / projection.z * 2.0 - 1.0,
            1.0 - (pos.y + projection.y) / projection.w * 2.0,
            0.0, 1.0);
        TexCoords = vec2(0.0);
        BgColor = aBgColor;
        TextColor = vec4(0.0);
        Colored = 0;
    } else {
        vec2 glyphSize = vec2(aGlyphRect.zw);
        vec2 glyphOffset = vec2(aGlyphRect.xy);
        vec2 pos = cellPos + glyphOffset + aQuadVertex * glyphSize;
        gl_Position = vec4(
            (pos.x + projection.x) / projection.z * 2.0 - 1.0,
            1.0 - (pos.y + projection.y) / projection.w * 2.0,
            0.0, 1.0);

        vec2 uv0 = aUVRect.xy;
        vec2 uv1 = aUVRect.xy + aUVRect.zw;
        TexCoords = mix(uv0, uv1, aQuadVertex);
        BgColor = aBgColor;
        TextColor = aFgColor;
        Colored = aColored;
    }
}
)";

static const char* TEXT_FS = R"(
#version 330 core
in vec2 TexCoords;
in vec4 TextColor;
in vec4 BgColor;
flat in int Colored;
uniform sampler2D mask;
uniform int renderingPass;

out vec4 FragColor;

void main() {
    if (renderingPass == 0) {
        FragColor = BgColor;
    } else {
        vec4 tex = texture(mask, TexCoords);
        if (Colored == 1) {
            FragColor = vec4(tex.rgb, tex.a);
        } else {
            FragColor = vec4(TextColor.rgb, tex.a);
        }
    }
}
)";

static const char* RECT_VS = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;

uniform vec4 projection;

out vec4 RectColor;

void main() {
    gl_Position = vec4(
        (aPos.x + projection.x) / projection.z * 2.0 - 1.0,
        1.0 - (aPos.y + projection.y) / projection.w * 2.0,
        0.0, 1.0);
    RectColor = aColor;
}
)";

static const char* RECT_FS = R"(
#version 330 core
in vec4 RectColor;
out vec4 FragColor;

void main() {
    FragColor = RectColor;
}
)";

// ─── Statics ────────────────────────────────────────────────────────────────

static const char* FONT_NAMES[] = {
    "Hack Nerd Font", "Hack Nerd Font Mono", "Menlo", "Monaco", "Courier New",
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

static bool isEmoji(wchar_t ch) {
    unsigned int cp = (unsigned int)ch;
    return (cp >= 0x1F600 && cp <= 0x1F64F) ||
           (cp >= 0x1F300 && cp <= 0x1F5FF) ||
           (cp >= 0x1F680 && cp <= 0x1F6FF) ||
           (cp >= 0x1F900 && cp <= 0x1F9FF) ||
           (cp >= 0x1FA00 && cp <= 0x1FAFF) ||
           (cp >= 0x2600  && cp <= 0x26FF)  ||
           (cp >= 0x2700  && cp <= 0x27BF)  ||
           (cp >= 0x2300  && cp <= 0x23FF)  ||
           (cp >= 0x2B05  && cp <= 0x2B55)  ||
           (cp >= 0x2934  && cp <= 0x2935)  ||
           (cp >= 0x3030  && cp <= 0x303D)  ||
           (cp >= 0x3297  && cp <= 0x3299);
}

static bool isNerdIcon(wchar_t ch) {
    unsigned int cp = (unsigned int)ch;
    return (cp >= 0xE000  && cp <= 0xE0D4) ||  // Powerline + extra
           (cp >= 0xE300  && cp <= 0xE3E3) ||  // Seti UI + extra
           (cp >= 0xE5FA  && cp <= 0xE6AC) ||  // Seti UI continued
           (cp >= 0xE700  && cp <= 0xE7C5) ||  // Devicons
           (cp >= 0xE000  && cp <= 0xE100) ||  // Miscellaneous PUA
           (cp >= 0xF000  && cp <= 0xF2FF) ||  // Private Use
           (cp >= 0xF400  && cp <= 0xF532) ||  // Private Use
           (cp >= 0xED00  && cp <= 0xEF8C);    // Private Use
}

static void drawBoxDrawing(CGContextRef ctx, wchar_t ch, int gw, int gh,
                           double baselineY) {
    double midX = gw / 2.0;
    double midY = gh / 2.0;
    double left = 2.0, right = gw - 2.0;
    double top = 2.0, bottom = gh - 2.0;

    CGContextSetLineWidth(ctx, 1.0);
    CGContextSetLineCap(ctx, kCGLineCapSquare);

    auto hline = [&]() { CGContextMoveToPoint(ctx, left, midY); CGContextAddLineToPoint(ctx, right, midY); };
    auto vline = [&]() { CGContextMoveToPoint(ctx, midX, top); CGContextAddLineToPoint(ctx, midX, bottom); };
    auto hleft = [&]() { CGContextMoveToPoint(ctx, left, midY); CGContextAddLineToPoint(ctx, midX, midY); };
    auto hright = [&]() { CGContextMoveToPoint(ctx, midX, midY); CGContextAddLineToPoint(ctx, right, midY); };
    auto vtop = [&]() { CGContextMoveToPoint(ctx, midX, top); CGContextAddLineToPoint(ctx, midX, midY); };
    auto vbot = [&]() { CGContextMoveToPoint(ctx, midX, midY); CGContextAddLineToPoint(ctx, midX, bottom); };

    switch (ch) {
    case 0x2500: hline(); break;
    case 0x2502: vline(); break;
    case 0x250C: hright(); vbot(); break;
    case 0x2510: hleft(); vbot(); break;
    case 0x2514: hright(); vtop(); break;
    case 0x2518: hleft(); vtop(); break;
    case 0x251C: hright(); vline(); break;
    case 0x2524: hleft(); vline(); break;
    case 0x252C: hline(); vbot(); break;
    case 0x2534: hline(); vtop(); break;
    case 0x253C: hline(); vline(); break;
    case 0x2501: hline(); break;
    case 0x2503: vline(); break;
    case 0x250F: hright(); vbot(); break;
    case 0x2513: hleft(); vbot(); break;
    case 0x2517: hright(); vtop(); break;
    case 0x251B: hleft(); vtop(); break;
    case 0x2523: hright(); vline(); break;
    case 0x252B: hleft(); vline(); break;
    case 0x252F: hline(); vbot(); break;
    case 0x2537: hline(); vtop(); break;
    case 0x253B: hline(); vline(); break;
    case 0x2550: hline(); break;
    case 0x2551: vline(); break;
    case 0x2554: hright(); vbot(); break;
    case 0x2557: hleft(); vbot(); break;
    case 0x255A: hright(); vtop(); break;
    case 0x255D: hleft(); vtop(); break;
    case 0x2560: hright(); vline(); break;
    case 0x2563: hleft(); vline(); break;
    case 0x2566: hline(); vbot(); break;
    case 0x2569: hline(); vtop(); break;
    case 0x256C: hline(); vline(); break;
    case 0x256D: hright(); vbot(); break;
    case 0x256E: hleft(); vbot(); break;
    case 0x256F: hleft(); vtop(); break;
    case 0x2570: hright(); vtop(); break;
    case 0x2571: CGContextMoveToPoint(ctx, right, top); CGContextAddLineToPoint(ctx, left, bottom); break;
    case 0x2572: CGContextMoveToPoint(ctx, left, top); CGContextAddLineToPoint(ctx, right, bottom); break;
    case 0x2573:
        CGContextMoveToPoint(ctx, right, top); CGContextAddLineToPoint(ctx, left, bottom);
        CGContextMoveToPoint(ctx, left, top); CGContextAddLineToPoint(ctx, right, bottom);
        break;
    case 0x2580: CGContextFillRect(ctx, CGRectMake(left, midY, right - left, bottom - midY)); break;
    case 0x2584: CGContextFillRect(ctx, CGRectMake(left, top, right - left, midY - top)); break;
    case 0x2588: CGContextFillRect(ctx, CGRectMake(left, top, right - left, bottom - top)); break;
    case 0x258C: CGContextFillRect(ctx, CGRectMake(left, top, (right - left) / 2.0, bottom - top)); break;
    case 0x2590: CGContextFillRect(ctx, CGRectMake(midX, top, right - midX, bottom - top)); break;
    default: break;
    }
    CGContextStrokePath(ctx);
}

// ─── cell_flags encoding ───────────────────────────────────────────────────
// bit 0: isColor (emoji)
// bits 1-2: fontStyle (0=normal, 1=bold, 2=italic, 3=bold+italic)

static uint8_t makeCellFlags(bool isColor, int fontStyle) {
    return (uint8_t)((isColor ? 1 : 0) | ((fontStyle & 0x3) << 1));
}

static bool cellFlagIsColor(uint8_t flags) { return (flags & 1) != 0; }
static int cellFlagStyle(uint8_t flags) { return (flags >> 1) & 0x3; }

// ─── Renderer ───────────────────────────────────────────────────────────────

Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

bool Renderer::init(GLFWwindow* window) {
    m_window = window;

    dbg("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));
    dbg("GL_RENDERER: %s\n", (const char*)glGetString(GL_RENDERER));

    if (!createShaders()) return false;

    int fbW, fbH, winW, winH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glfwGetWindowSize(window, &winW, &winH);

    for (int style = 0; style < NUM_STYLES; style++) {
        if (!createFontAtlas(fbW, fbH, winW, winH, style)) {
            dbg("createFontAtlas FAILED for style %d\n", style);
            return false;
        }
    }

    // Text VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    // Quad vertices (2 triangles)
    float quadVerts[] = { 0,0, 1,0, 1,1, 0,1 };
    unsigned short quadIdx[] = { 0,1,2, 0,2,3 };

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TextInstance) * BATCH_MAX, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIdx), quadIdx, GL_STATIC_DRAW);

    // location 0: quad vertex
    GLuint quadVbo;
    glGenBuffers(1, &quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Instance attributes (locations 1-6)
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    GLsizei stride = sizeof(TextInstance);
    // aCellPos (ivec2)
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 2, GL_SHORT, stride, (void*)offsetof(TextInstance, col));
    glVertexAttribDivisor(1, 1);
    // aGlyphRect (ivec4)
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 4, GL_SHORT, stride, (void*)offsetof(TextInstance, left));
    glVertexAttribDivisor(2, 1);
    // aUVRect (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(TextInstance, uv_left));
    glVertexAttribDivisor(3, 1);
    // aFgColor (vec4 as bytes -> normalized)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(TextInstance, r));
    glVertexAttribDivisor(4, 1);
    // aBgColor (vec4 as bytes -> normalized)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(TextInstance, bg_r));
    glVertexAttribDivisor(5, 1);
    // aColored (int)
    glEnableVertexAttribArray(6);
    glVertexAttribIPointer(6, 1, GL_UNSIGNED_BYTE, stride, (void*)offsetof(TextInstance, cell_flags));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);

    // Rect VAO
    glGenVertexArrays(1, &m_rectVao);
    glBindVertexArray(m_rectVao);

    glGenBuffers(1, &m_rectVbo);
    glGenBuffers(1, &m_rectEbo);
    glGenBuffers(1, &m_rectInstanceVbo);

    unsigned short rectIdx[] = { 0,1,2, 0,2,3 };
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rectEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectIdx), rectIdx, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, m_rectInstanceVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(RectInstance) * 4096, nullptr, GL_DYNAMIC_DRAW);

    // aPos (vec2) — per-vertex dummy quad
    float dummyVerts[] = {0,0, 1,0, 1,1, 0,1};
    GLuint dummyVbo;
    glGenBuffers(1, &dummyVbo);
    glBindBuffer(GL_ARRAY_BUFFER, dummyVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummyVerts), dummyVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Per-instance: position + color
    glBindBuffer(GL_ARRAY_BUFFER, m_rectInstanceVbo);
    GLsizei rstride = sizeof(RectInstance);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, rstride, (void*)offsetof(RectInstance, x));
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, rstride, (void*)offsetof(RectInstance, r));
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_initialized = true;
    dbg("Renderer init OK (GL3 core), cell=%dx%d\n", m_cellWidth, m_cellHeight);
    return true;
}

bool Renderer::createShaders() {
    if (!m_textShader.compile(TEXT_VS, TEXT_FS)) return false;
    m_textProjLoc = m_textShader.loc("projection");
    m_textCellSizeLoc = m_textShader.loc("cellSize");
    m_textPassLoc = m_textShader.loc("renderingPass");
    m_textMaskLoc = m_textShader.loc("mask");

    if (!m_rectShader.compile(RECT_VS, RECT_FS)) return false;
    m_rectProjLoc = m_rectShader.loc("projection");

    dbg("Shaders compiled OK\n");
    return true;
}

void Renderer::shutdown() {
    for (int i = 0; i < NUM_STYLES; i++) {
        if (m_fontTexture[i]) { glDeleteTextures(1, &m_fontTexture[i]); m_fontTexture[i] = 0; }
    }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    if (m_rectVao) { glDeleteVertexArrays(1, &m_rectVao); m_rectVao = 0; }
    if (m_rectVbo) { glDeleteBuffers(1, &m_rectVbo); m_rectVbo = 0; }
    if (m_rectEbo) { glDeleteBuffers(1, &m_rectEbo); m_rectEbo = 0; }
    if (m_rectInstanceVbo) { glDeleteBuffers(1, &m_rectInstanceVbo); m_rectInstanceVbo = 0; }
    if (m_textShader.id) { glDeleteProgram(m_textShader.id); m_textShader.id = 0; }
    if (m_rectShader.id) { glDeleteProgram(m_rectShader.id); m_rectShader.id = 0; }
    m_initialized = false;
}

bool Renderer::createFontAtlas(int fbW, int fbH, int winW, int winH, int fontStyle) {
    float scaleFactor = (float)fbW / (float)winW;
    if (scaleFactor < 1.0f) scaleFactor = 1.0f;

    const double fontSize = 16.0;
    const double atlasFontSize = fontSize * scaleFactor;
    CTFontRef ctFont = nullptr;

    const char* fontStyleSuffix = "";
    CTFontSymbolicTraits traits = 0;
    switch (fontStyle) {
    case 0: fontStyleSuffix = ""; traits = 0; break;
    case 1: fontStyleSuffix = " Bold"; traits = kCTFontTraitBold; break;
    case 2: fontStyleSuffix = " Italic"; traits = kCTFontTraitItalic; break;
    case 3: fontStyleSuffix = " Bold Italic"; traits = kCTFontTraitBold | kCTFontTraitItalic; break;
    }

    for (const char* name : FONT_NAMES) {
        std::string fontNameStr = std::string(name) + fontStyleSuffix;
        CFReleaser fontNameHolder(CFStringCreateWithCString(kCFAllocatorDefault, fontNameStr.c_str(), kCFStringEncodingUTF8));
        if (!fontNameHolder.get()) continue;
        
        CTFontRef trialFont = CTFontCreateWithName(static_cast<CFStringRef>(fontNameHolder.get()), atlasFontSize, nullptr);
        if (trialFont) {
            UniChar xc = 'X';
            CGGlyph g = 0;
            CTFontGetGlyphsForCharacters(trialFont, &xc, &g, 1);
            CGFloat w = CTFontGetAdvancesForGlyphs(trialFont, kCTFontOrientationDefault, &g, nullptr, 1);
            if (w >= 4.0 * scaleFactor) {
                dbg("Font: %s (%.0fpt)\n", name, atlasFontSize);
                if (ctFont) CFRelease(ctFont);
                ctFont = trialFont;
                break;
            }
            CFRelease(trialFont);
        }
    }
    if (!ctFont) {
        CFStringRef fn = CFStringCreateWithCString(kCFAllocatorDefault, "Menlo", kCFStringEncodingUTF8);
        if (fn) {
            ctFont = CTFontCreateWithName(fn, atlasFontSize, nullptr);
            CFRelease(fn);
        }
    }
    if (!ctFont) return false;

    UniChar xc = 'X';
    CGGlyph glyphX = 0;
    CTFontGetGlyphsForCharacters(ctFont, &xc, &glyphX, 1);
    double advance = CTFontGetAdvancesForGlyphs(ctFont, kCTFontOrientationDefault, &glyphX, nullptr, 1);
    double ascent = CTFontGetAscent(ctFont);
    double descent = CTFontGetDescent(ctFont);
    double leading = CTFontGetLeading(ctFont);

    m_cellWidth = (int)round(advance / scaleFactor);
    m_cellHeight = (int)round((ascent + descent + leading) / scaleFactor);
    if (m_cellWidth < 1) m_cellWidth = 8;
    if (m_cellHeight < 1) m_cellHeight = 16;

    int atlasCellW = (int)ceil(advance) + 4;
    int atlasCellH = (int)ceil(ascent + descent + leading) + 4;

    double baselineY = descent + leading / 2.0 + 2.0;

    // Collect characters
    std::vector<wchar_t> chars;
    for (wchar_t c = 0x20; c < 0x80; c++) chars.push_back(c);
    for (wchar_t c = 0xA0; c < 0x250; c++) chars.push_back(c);
    for (wchar_t c = 0x370; c < 0x400; c++) chars.push_back(c);
    for (wchar_t c = 0x400; c < 0x530; c++) chars.push_back(c);
    for (wchar_t c = 0x2190; c < 0x2200; c++) chars.push_back(c);
    for (wchar_t c = 0x2200; c < 0x2300; c++) chars.push_back(c);
    for (wchar_t c = 0x2500; c < 0x25A0; c++) chars.push_back(c);
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
    for (wchar_t c = 0x1F300; c <= 0x1F5FF; c++) chars.push_back(c);
    for (wchar_t c = 0x1F600; c <= 0x1F64F; c++) chars.push_back(c);
    for (wchar_t c = 0x1F680; c <= 0x1F6FF; c++) chars.push_back(c);
    for (wchar_t c = 0x1F900; c <= 0x1F9FF; c++) chars.push_back(c);
    for (wchar_t c = 0x1FA00; c <= 0x1FAFF; c++) chars.push_back(c);
    for (wchar_t c = 0x2300; c <= 0x23FF; c++) chars.push_back(c);
    for (wchar_t c = 0x2B05; c <= 0x2B55; c++) chars.push_back(c);

    std::sort(chars.begin(), chars.end());
    chars.erase(std::unique(chars.begin(), chars.end()), chars.end());

    CFStringRef fontAttrKey = kCTFontAttributeName;
    CFDictionaryRef fontAttr = CFDictionaryCreate(kCFAllocatorDefault,
        (const void**)&fontAttrKey, (const void**)&ctFont, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    struct RenderedGlyph {
        wchar_t ch;
        CGGlyph glyph;
        CTFontRef font;
        CGFloat adv, bearingX, bearingY, bbW, bbH;
        bool isBoxDrawing, isEmoji;
    };
    std::vector<RenderedGlyph> rendered;
    std::vector<wchar_t> missing;
    std::unordered_map<wchar_t, CGGlyph> directGlyphs;

    for (wchar_t ch : chars) {
        UniChar uc = (UniChar)ch;
        CGGlyph glyph = 0;
        CTFontGetGlyphsForCharacters(ctFont, &uc, &glyph, 1);
        if (glyph != 0) directGlyphs[ch] = glyph;
        else missing.push_back(ch);
    }

    std::unordered_map<wchar_t, std::pair<CGGlyph, CTFontRef>> fallbackGlyphs;
    std::vector<CTFontRef> retainedFonts;
    if (!missing.empty()) {
        std::vector<UniChar> missUni(missing.begin(), missing.end());
        CFStringRef missStr = CFStringCreateWithCharacters(kCFAllocatorDefault, missUni.data(), missUni.size());
        CFAttributedStringRef missAttrStr = CFAttributedStringCreate(kCFAllocatorDefault, missStr, fontAttr);
        CTLineRef missLine = CTLineCreateWithAttributedString(missAttrStr);
        
        if (missLine) {
            CFArrayRef runs = CTLineGetGlyphRuns(missLine);
            CFIndex runCount = CFArrayGetCount(runs);
            CFIndex charIdx = 0;

            for (CFIndex r = 0; r < runCount && charIdx < (CFIndex)missing.size(); r++) {
                CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, r);
                CFIndex glyphCount = CTRunGetGlyphCount(run);
                CFDictionaryRef runAttrs = CTRunGetAttributes(run);
                CTFontRef runFont = ctFont;
                CFDictionaryGetValueIfPresent(runAttrs, kCTFontAttributeName, (const void**)&runFont);
                if (runFont != ctFont) { CFRetain(runFont); retainedFonts.push_back(runFont); }

                std::vector<CGGlyph> runGlyphs(glyphCount);
                CTRunGetGlyphs(run, CFRangeMake(0, glyphCount), runGlyphs.data());
                for (CFIndex g = 0; g < glyphCount && charIdx < (CFIndex)missing.size(); g++, charIdx++) {
                    if (runGlyphs[g] != 0)
                        fallbackGlyphs[missing[charIdx]] = {runGlyphs[g], runFont};
                }
            }
            CFRelease(missLine);
        }
        if (missAttrStr) CFRelease(missAttrStr);
        if (missStr) CFRelease(missStr);
    }

    for (wchar_t ch : chars) {
        CGGlyph glyph = 0;
        CTFontRef useFont = ctFont;
        auto itD = directGlyphs.find(ch);
        if (itD != directGlyphs.end()) { glyph = itD->second; }
        else {
            auto itF = fallbackGlyphs.find(ch);
            if (itF != fallbackGlyphs.end()) { glyph = itF->second.first; useFont = itF->second.second; }
        }
        if (glyph == 0) continue;

        CGRect bbRect;
        CTFontGetBoundingRectsForGlyphs(useFont, kCTFontOrientationDefault, &glyph, &bbRect, 1);
        CGFloat advVal = CTFontGetAdvancesForGlyphs(useFont, kCTFontOrientationDefault, &glyph, nullptr, 1);
        bool isEmojiChar = isEmoji(ch);

        rendered.push_back({ch, glyph, useFont, advVal,
            bbRect.origin.x, bbRect.origin.y, bbRect.size.width, bbRect.size.height,
            (ch >= 0x2500 && ch <= 0x259F), isEmojiChar});
    }

    CFRelease(fontAttr);

    int totalChars = (int)rendered.size();
    int rows = (totalChars + ATLAS_COLS - 1) / ATLAS_COLS;
    m_atlasTexW = ATLAS_COLS * atlasCellW;
    m_atlasTexH = rows * atlasCellH;
    auto nextPow2 = [](int v) { int p = 1; while (p < v) p <<= 1; return p; };
    m_atlasTexW = nextPow2(m_atlasTexW);
    m_atlasTexH = nextPow2(m_atlasTexH);

    std::vector<uint8_t> rgba(m_atlasTexW * m_atlasTexH * 4, 0);

    for (int i = 0; i < totalChars; i++) {
        const RenderedGlyph& rg = rendered[i];
        int col = i % ATLAS_COLS;
        int row = i / ATLAS_COLS;
        int px = col * atlasCellW;
        int py = row * atlasCellH;
        int gw = atlasCellW;
        int gh = atlasCellH;

        size_t bpc = 8;
        size_t bpr = gw * 4;
        std::vector<uint8_t> bitmap(gw * gh * 4, 0);

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(bitmap.data(), gw, gh, bpc, bpr, cs, kCGImageAlphaPremultipliedLast);
        
        if (ctx) {
            CGContextSetRGBFillColor(ctx, 1.0, 1.0, 1.0, 1.0);
            CGContextSetTextDrawingMode(ctx, kCGTextFill);

            if (rg.isBoxDrawing) {
                drawBoxDrawing(ctx, rg.ch, gw, gh, baselineY);
            } else if (rg.isEmoji) {
                CGFloat emojiScale = 1.0;
                if (rg.bbW > 0 && rg.bbH > 0) {
                    double maxDim = std::min((double)(gw - 4), (double)(gh - 4));
                    double glyphMax = std::max((double)rg.bbW, (double)rg.bbH);
                    if (glyphMax > maxDim) emojiScale = maxDim / glyphMax;
                }
                CGContextSaveGState(ctx);
                double dx = (gw - rg.bbW * emojiScale) / 2.0 + rg.bearingX * emojiScale;
                double dy = baselineY + rg.bearingY * emojiScale;
                CGContextTranslateCTM(ctx, dx, dy);
                CGContextScaleCTM(ctx, emojiScale, emojiScale);
                CTFontDrawGlyphs(rg.font, &rg.glyph, &CGPointZero, 1, ctx);
                CGContextRestoreGState(ctx);
            } else {
                CGFloat gx = (gw - rg.adv) / 2.0;
                CGPoint point = {gx, (CGFloat)baselineY};
                CTFontDrawGlyphs(rg.font, &rg.glyph, &point, 1, ctx);
            }
            CGContextRelease(ctx);
            CGColorSpaceRelease(cs);
        }

        for (int srcY = 0; srcY < gh && (py + srcY) < m_atlasTexH; srcY++) {
            for (int srcX = 0; srcX < gw && (px + srcX) < m_atlasTexW; srcX++) {
                int si = (srcY * gw + srcX) * 4;
                int di = ((py + gh - 1 - srcY) * m_atlasTexW + (px + srcX)) * 4;
                if (rg.isEmoji) {
                    rgba[di+0] = bitmap[si+0]; rgba[di+1] = bitmap[si+1];
                    rgba[di+2] = bitmap[si+2]; rgba[di+3] = bitmap[si+3];
                } else {
                    uint8_t r = bitmap[si+0], g = bitmap[si+1], b = bitmap[si+2], a = bitmap[si+3];
                    uint8_t lum = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
                    if (a > lum) lum = a;
                    rgba[di+0] = 255; rgba[di+1] = 255; rgba[di+2] = 255; rgba[di+3] = lum;
                }
            }
        }

        // Compute glyph bounds — box drawing uses full cell, others use font metrics
        double renderX, glyphLeft, glyphBottom, glyphTop, glyphRight;
        if (rg.isBoxDrawing) {
            // Box drawing fills the full cell (like Alacritty builtin_box_drawing)
            glyphLeft = 2.0;
            glyphBottom = 2.0;
            glyphRight = gw - 2.0;
            glyphTop = gh - 2.0;
            renderX = 0;
        } else if (rg.isEmoji) {
            CGFloat es = 1.0;
            if (rg.bbW > 0 && rg.bbH > 0) {
                double md = std::min((double)(gw-4), (double)(gh-4));
                double gm = std::max((double)rg.bbW, (double)rg.bbH);
                if (gm > md) es = md / gm;
            }
            renderX = (gw - rg.bbW * es) / 2.0;
            glyphLeft = renderX + rg.bearingX * es;
            glyphBottom = baselineY + rg.bearingY * es;
            glyphTop = glyphBottom + rg.bbH * es;
            glyphRight = glyphLeft + rg.bbW * es;
        } else {
            renderX = (gw - rg.adv) / 2.0;
            glyphLeft = renderX + rg.bearingX;
            glyphBottom = baselineY + rg.bearingY;
            glyphTop = glyphBottom + rg.bbH;
            glyphRight = glyphLeft + rg.bbW;
        }

        if (glyphLeft < 0) glyphLeft = 0;
        if (glyphBottom < 0) glyphBottom = 0;
        if (glyphRight > gw) glyphRight = gw;
        if (glyphTop > gh) glyphTop = gh;

        GlyphInfo gi = {};
        gi.u0 = (float)(px + glyphLeft) / m_atlasTexW;
        gi.v0 = (float)(py + glyphTop) / m_atlasTexH;
        gi.u1 = (float)(px + glyphRight) / m_atlasTexW;
        gi.v1 = (float)(py + glyphBottom) / m_atlasTexH;
        gi.isColor = rg.isEmoji;

        if (rg.isBoxDrawing) {
            // Box drawing fills the full cell
            gi.glyphW = (float)m_cellWidth;
            gi.glyphH = (float)m_cellHeight;
            gi.bearX = 0.0f;
            gi.bearY = 0.0f;
        } else if (rg.isEmoji) {
            CGFloat es = 1.0;
            if (rg.bbW > 0 && rg.bbH > 0) {
                double md = std::min((double)(gw-4), (double)(gh-4));
                double gm = std::max((double)rg.bbW, (double)rg.bbH);
                if (gm > md) es = md / gm;
            }
            gi.glyphW = (float)(rg.bbW * es) / scaleFactor;
            gi.glyphH = (float)(rg.bbH * es) / scaleFactor;
            gi.bearX = (float)((gw - rg.bbW * es) / 2.0 + rg.bearingX * es) / scaleFactor;
            gi.bearY = (float)(baselineY + rg.bearingY * es) / scaleFactor;
        } else {
            gi.glyphW = (float)rg.bbW / scaleFactor;
            gi.glyphH = (float)rg.bbH / scaleFactor;
            gi.bearX = (float)(renderX + rg.bearingX) / scaleFactor;
            gi.bearY = (float)(baselineY + rg.bearingY) / scaleFactor;
        }
        m_glyphs[fontStyle][rg.ch] = gi;
    }

    CFRelease(ctFont);
    for (CTFontRef f : retainedFonts) CFRelease(f);

    glGenTextures(1, &m_fontTexture[fontStyle]);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture[fontStyle]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_atlasTexW, m_atlasTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    dbg("Atlas style %d: %d glyphs, tex=%dx%d\n", fontStyle, totalChars, m_atlasTexW, m_atlasTexH);
    return true;
}

// ─── Frame ──────────────────────────────────────────────────────────────────

void Renderer::beginFrame(int fbWidth, int fbHeight, int winWidth, int winHeight) {
    if (!m_initialized) return;

    m_fbWidth = fbWidth;
    m_fbHeight = fbHeight;
    m_winWidth = winWidth;
    m_winHeight = winHeight;

    m_projOffsetX = 0.0f;
    m_projOffsetY = 0.0f;
    m_projScaleX = (float)fbWidth;
    m_projScaleY = (float)fbHeight;

    glViewport(0, 0, fbWidth, fbHeight);
    glClearColor(0.102f, 0.106f, 0.149f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_textBatch.clear();
    m_rectBatch.clear();
    m_cursorBatch.clear();
}

void Renderer::drawCell(int x, int y, wchar_t ch, uint32_t fg, uint32_t bg,
                        bool bold, bool italic) {
    // Background rect — positions in fb pixels
    float r = ((bg >> 16) & 0xFF) / 255.0f;
    float g = ((bg >> 8) & 0xFF) / 255.0f;
    float b = (bg & 0xFF) / 255.0f;
    float scaleX = m_projScaleX / (float)m_winWidth;
    float scaleY = m_projScaleY / (float)m_winHeight;
    m_rectBatch.push_back({(float)x * m_cellWidth * scaleX, (float)y * m_cellHeight * scaleY,
                           (float)m_cellWidth * scaleX, (float)m_cellHeight * scaleY,
                           r, g, b, 1.0f});

    if (ch == L' ' || ch == 0) return;

    int style = 0;
    if (bold && italic) style = 3;
    else if (bold) style = 1;
    else if (italic) style = 2;

    auto it = m_glyphs[style].find(ch);
    if (it == m_glyphs[style].end()) { it = m_glyphs[0].find(ch); style = 0; }
    if (it == m_glyphs[0].end()) return;
    const GlyphInfo& gi = it->second;

    TextInstance inst;
    inst.col = (int16_t)x;
    inst.row = (int16_t)y;
    inst.left = (int16_t)(gi.bearX * m_fbWidth / m_winWidth);
    inst.top = (int16_t)((m_cellHeight - gi.bearY - gi.glyphH) * m_fbHeight / m_winHeight);
    inst.width = (int16_t)(gi.glyphW * m_fbWidth / m_winWidth);
    inst.height = (int16_t)(gi.glyphH * m_fbHeight / m_winHeight);
    inst.uv_left = gi.u0;
    inst.uv_bot = gi.v0;
    inst.uv_width = gi.u1 - gi.u0;
    inst.uv_height = gi.v1 - gi.v0;
    inst.r = (fg >> 16) & 0xFF;
    inst.g = (fg >> 8) & 0xFF;
    inst.b = fg & 0xFF;
    inst.cell_flags = makeCellFlags(gi.isColor, style);
    inst.bg_r = (bg >> 16) & 0xFF;
    inst.bg_g = (bg >> 8) & 0xFF;
    inst.bg_b = bg & 0xFF;
    inst.bg_a = 255;

    if ((int)m_textBatch.size() >= BATCH_MAX) flushTextBatch();
    m_textBatch.push_back(inst);
}

void Renderer::drawCursor(int x, int y, int cellW, int cellH, uint32_t color) {
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    float scaleX = m_projScaleX / (float)m_winWidth;
    float scaleY = m_projScaleY / (float)m_winHeight;
    m_cursorBatch.push_back({(float)x * cellW * scaleX, (float)y * cellH * scaleY,
                           (float)cellW * scaleX, (float)cellH * scaleY,
                           r, g, b, 1.0f});
}

void Renderer::flushBatches() {
    if (!m_initialized) return;
    flushRectBatch();
    flushTextBatch();
    // Cursor drawn last so it's on top of text
    if (!m_cursorBatch.empty()) {
        std::swap(m_rectBatch, m_cursorBatch);
        flushRectBatch();
        m_cursorBatch.clear();
    }
}

void Renderer::present() {
    if (!m_initialized || !m_window) return;
    glfwSwapBuffers(m_window);
}

// ─── Flush ──────────────────────────────────────────────────────────────────

void Renderer::flushTextBatch() {
    if (m_textBatch.empty()) return;

    m_textShader.use();
    glUniform4f(m_textProjLoc, m_projOffsetX, m_projOffsetY, m_projScaleX, m_projScaleY);
    // cellSize in fb pixels: cellWidth * fbWidth/winWidth, cellHeight * fbHeight/winHeight
    float cellSizeFbX = (float)m_cellWidth * m_projScaleX / (float)m_winWidth;
    float cellSizeFbY = (float)m_cellHeight * m_projScaleY / (float)m_winHeight;
    glUniform2f(m_textCellSizeLoc, cellSizeFbX, cellSizeFbY);
    glUniform1i(m_textMaskLoc, 0);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Sort batch by style for efficient texture-binding
    std::sort(m_textBatch.begin(), m_textBatch.end(), [](const TextInstance& a, const TextInstance& b) {
        return cellFlagStyle(a.cell_flags) < cellFlagStyle(b.cell_flags);
    });

    // Draw per-style ranges (0=normal, 1=bold, 2=italic, 3=bold+italic)
    // Upload each range separately so instanced draw reads from offset 0
    int start = 0;
    while (start < (int)m_textBatch.size()) {
        int style = cellFlagStyle(m_textBatch[start].cell_flags);
        int end = start + 1;
        while (end < (int)m_textBatch.size() && cellFlagStyle(m_textBatch[end].cell_flags) == style)
            end++;

        int count = end - start;

        // Upload only this range's instances
        glBufferData(GL_ARRAY_BUFFER, count * sizeof(TextInstance),
                     &m_textBatch[start], GL_DYNAMIC_DRAW);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_fontTexture[style]);

        // Background pass
        glUniform1i(m_textPassLoc, 0);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, count);

        // Text pass
        glUniform1i(m_textPassLoc, 1);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, count);

        start = end;
    }

    glBindVertexArray(0);
    glUseProgram(0);
    m_textBatch.clear();
}

void Renderer::flushRectBatch() {
    if (m_rectBatch.empty()) return;

    m_rectShader.use();
    glUniform4f(m_rectProjLoc, m_projOffsetX, m_projOffsetY, m_projScaleX, m_projScaleY);

    glBindVertexArray(m_rectVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectInstanceVbo);
    glBufferData(GL_ARRAY_BUFFER, m_rectBatch.size() * sizeof(RectInstance),
                 m_rectBatch.data(), GL_DYNAMIC_DRAW);

    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, (GLsizei)m_rectBatch.size());

    glBindVertexArray(0);
    glUseProgram(0);
    m_rectBatch.clear();
}
