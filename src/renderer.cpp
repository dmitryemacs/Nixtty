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

static void drawBoxDrawing(CGContextRef ctx, wchar_t ch, int gw, int gh,
                           double baselineY, double ascent, double descent, double advance) {
    double midX = gw / 2.0;
    double midY = gh / 2.0;
    double left = 2.0;
    double right = gw - 2.0;
    double top = 2.0;
    double bottom = gh - 2.0;

    CGContextSetLineWidth(ctx, 1.0);
    CGContextSetLineCap(ctx, kCGLineCapSquare);

    auto hline = [&]() {
        CGContextMoveToPoint(ctx, left, midY);
        CGContextAddLineToPoint(ctx, right, midY);
    };
    auto vline = [&]() {
        CGContextMoveToPoint(ctx, midX, top);
        CGContextAddLineToPoint(ctx, midX, bottom);
    };
    auto hleft = [&]() {
        CGContextMoveToPoint(ctx, left, midY);
        CGContextAddLineToPoint(ctx, midX, midY);
    };
    auto hright = [&]() {
        CGContextMoveToPoint(ctx, midX, midY);
        CGContextAddLineToPoint(ctx, right, midY);
    };
    auto vtop = [&]() {
        CGContextMoveToPoint(ctx, midX, top);
        CGContextAddLineToPoint(ctx, midX, midY);
    };
    auto vbot = [&]() {
        CGContextMoveToPoint(ctx, midX, midY);
        CGContextAddLineToPoint(ctx, midX, bottom);
    };

    switch (ch) {
        case 0x2500: hline(); break;             // ─
        case 0x2502: vline(); break;             // │
        case 0x250C: hright(); vbot(); break;    // ┌
        case 0x2510: hleft(); vbot(); break;     // ┐
        case 0x2514: hright(); vtop(); break;    // └
        case 0x2518: hleft(); vtop(); break;     // ┘
        case 0x251C: hright(); vline(); break;   // ├
        case 0x2524: hleft(); vline(); break;    // ┤
        case 0x252C: hline(); vbot(); break;     // ┬
        case 0x2534: hline(); vtop(); break;     // ┴
        case 0x253C: hline(); vline(); break;    // ┼
        case 0x2501: hline(); break;             // ━ (bold horizontal)
        case 0x2503: vline(); break;             // ┃ (bold vertical)
        case 0x250F: hright(); vbot(); break;    // ┏
        case 0x2513: hleft(); vbot(); break;     // ┓
        case 0x2517: hright(); vtop(); break;    // ┗
        case 0x251B: hleft(); vtop(); break;     // ┛
        case 0x2523: hright(); vline(); break;   // ┣
        case 0x252B: hleft(); vline(); break;    // ┫
        case 0x252F: hline(); vbot(); break;     // ┳
        case 0x2537: hline(); vtop(); break;     // ┻
        case 0x253B: hline(); vline(); break;    // ┋
        case 0x2506: vline(); break;             // ┆ (dotted vertical)
        case 0x2504: hline(); break;             // ┄ (dotted horizontal)
        case 0x2508: hline(); break;             // ┈ (centered horizontal)
        case 0x250A: vline(); break;             // ┊ (centered vertical)
        case 0x250E: hright(); vbot(); break;    // ┎ (top-left corner, no bottom)
        case 0x2512: hleft(); vbot(); break;     // ┒
        case 0x2516: hright(); vtop(); break;    // ┖
        case 0x251A: hleft(); vtop(); break;     // ┚
        case 0x2522: hright(); vline(); break;   // ┒
        case 0x252A: hleft(); vline(); break;    // ┚
        case 0x252E: hline(); vbot(); break;     // ┮
        case 0x2536: hline(); vtop(); break;     // ┶
        case 0x253A: hline(); vline(); break;    // ┺
        case 0x253E: hline(); vline(); break;    // ┾
        case 0x2540: hline(); vline(); break;    // ─
        case 0x2541: hline(); vline(); break;    // ─
        case 0x2542: hline(); vline(); break;    // ─
        case 0x2543: hline(); vline(); break;    // ─
        case 0x2544: hline(); break;             // ─
        case 0x2545: hline(); break;             // ─
        case 0x2546: hline(); break;             // ─
        case 0x2547: hline(); break;             // ─
        case 0x2548: hline(); break;             // ─
        case 0x2549: hline(); break;             // ─
        case 0x254A: hline(); break;             // ─
        case 0x254B: hline(); vline(); break;    // ┋
        case 0x254C: hline(); break;             // ─
        case 0x254D: hline(); break;             // ─
        case 0x254E: vline(); break;             // ┃
        case 0x254F: vline(); break;             // ┃
        case 0x2550: hline(); break;             // ═
        case 0x2551: vline(); break;             // ║
        case 0x2552: hleft(); vbot(); break;     // ╒
        case 0x2553: hright(); vtop(); break;    // ╓
        case 0x2554: hright(); vbot(); break;    // ╔
        case 0x2555: hleft(); vbot(); break;     // ╕
        case 0x2556: hleft(); vbot(); break;     // ╖
        case 0x2557: hleft(); vbot(); break;     // ╗
        case 0x2558: hright(); vtop(); break;    // ╘
        case 0x2559: hright(); vtop(); break;    // ╙
        case 0x255A: hright(); vtop(); break;    // ╚
        case 0x255B: hleft(); vtop(); break;     // ╛
        case 0x255C: hleft(); vtop(); break;     // ╜
        case 0x255D: hleft(); vtop(); break;     // ╝
        case 0x255E: hright(); vline(); break;   // ╞
        case 0x255F: hright(); vline(); break;   // ╟
        case 0x2560: hright(); vline(); break;   // ╠
        case 0x2561: hleft(); vline(); break;    // ╡
        case 0x2562: hleft(); vline(); break;    // ╢
        case 0x2563: hleft(); vline(); break;    // ╣
        case 0x2564: hline(); vbot(); break;     // ╤
        case 0x2565: hline(); vbot(); break;     // ╥
        case 0x2566: hline(); vbot(); break;     // ╦
        case 0x2567: hline(); vtop(); break;     // ╧
        case 0x2568: hline(); vtop(); break;     // ╨
        case 0x2569: hline(); vtop(); break;     // ╩
        case 0x256A: hline(); vline(); break;    // ╪
        case 0x256B: hline(); vline(); break;    // ╫
        case 0x256C: hline(); vline(); break;    // ╬
        case 0x256D: hright(); vbot(); break;    // ╭
        case 0x256E: hleft(); vbot(); break;     // ╮
        case 0x256F: hleft(); vtop(); break;     // ╯
        case 0x2570: hright(); vtop(); break;    // ╰
        case 0x2571:                           // ╱
            CGContextMoveToPoint(ctx, right, top);
            CGContextAddLineToPoint(ctx, left, bottom);
            break;
        case 0x2572:                           // ╲
            CGContextMoveToPoint(ctx, left, top);
            CGContextAddLineToPoint(ctx, right, bottom);
            break;
        case 0x2573:                           // ╳
            CGContextMoveToPoint(ctx, right, top);
            CGContextAddLineToPoint(ctx, left, bottom);
            CGContextMoveToPoint(ctx, left, top);
            CGContextAddLineToPoint(ctx, right, bottom);
            break;
        case 0x2574: hleft(); break;             // ╴
        case 0x2575: vtop(); break;              // ╵
        case 0x2576: hright(); break;            // ╶
        case 0x2577: vbot(); break;              // ╷
        case 0x2578: hleft(); break;             // ╸
        case 0x2579: vtop(); break;              // ╹
        case 0x257A: hright(); break;            // ╺
        case 0x257B: vbot(); break;              // ╻
        case 0x257C: hleft(); hright(); break;   // ╼
        case 0x257D: vtop(); vbot(); break;      // ╽
        case 0x257E: hright(); vbot(); break;    // ╾
        case 0x257F: hleft(); vtop(); break;     // ╿
        // Block elements
        case 0x2580: // ▀ Upper half block
            CGContextFillRect(ctx, CGRectMake(left, midY, right - left, bottom - midY));
            break;
        case 0x2584: // ▄ Lower half block
            CGContextFillRect(ctx, CGRectMake(left, top, right - left, midY - top));
            break;
        case 0x2588: // █ Full block
            CGContextFillRect(ctx, CGRectMake(left, top, right - left, bottom - top));
            break;
        case 0x2589: // ▉ Left seven eighths block
            CGContextFillRect(ctx, CGRectMake(left, top, (right - left) * 7.0/8.0, bottom - top));
            break;
        case 0x258A: // ▊ Left three quarters block
            CGContextFillRect(ctx, CGRectMake(left, top, (right - left) * 3.0/4.0, bottom - top));
            break;
        case 0x258B: // ▋ Left five eighths block
            CGContextFillRect(ctx, CGRectMake(left, top, (right - left) * 5.0/8.0, bottom - top));
            break;
        case 0x258C: // ▌ Left half block
            CGContextFillRect(ctx, CGRectMake(left, top, (right - left) / 2.0, bottom - top));
            break;
        case 0x258D: // ▍ Left three eighths block
            CGContextFillRect(ctx, CGRectMake(left, top, (right - left) * 3.0/8.0, bottom - top));
            break;
        case 0x258E: // ▎ Left one quarter block
            CGContextFillRect(ctx, CGRectMake(left, top, (right - left) / 4.0, bottom - top));
            break;
        case 0x258F: // ▏ Left one eighth block
            CGContextFillRect(ctx, CGRectMake(left, top, (right - left) / 8.0, bottom - top));
            break;
        case 0x2590: // ▐ Right half block
            CGContextFillRect(ctx, CGRectMake(midX, top, right - midX, bottom - top));
            break;
        case 0x2591: // ░ Light shade (25%)
            for (int sy = (int)top; sy < (int)bottom; sy += 4) {
                for (int sx = (int)left; sx < (int)right; sx += 4) {
                    CGContextFillRect(ctx, CGRectMake(sx, sy, 1, 1));
                }
            }
            break;
        case 0x2592: // ▒ Medium shade (50%)
            for (int sy = (int)top; sy < (int)bottom; sy += 2) {
                for (int sx = (int)left; sx < (int)right; sx += 2) {
                    if ((sx + sy) % 4 == 0)
                        CGContextFillRect(ctx, CGRectMake(sx, sy, 1, 1));
                }
            }
            break;
        case 0x2593: // ▓ Dark shade (75%)
            for (int sy = (int)top; sy < (int)bottom; sy += 2) {
                for (int sx = (int)left; sx < (int)right; sx += 2) {
                    if ((sx + sy) % 4 != 0)
                        CGContextFillRect(ctx, CGRectMake(sx, sy, 1, 1));
                }
            }
            break;
        case 0x2594: // ▔ Upper one eighth block
            CGContextFillRect(ctx, CGRectMake(left, bottom - (bottom - top)/8.0, right - left, (bottom - top)/8.0));
            break;
        case 0x2595: // ▕ Right one eighth block
            CGContextFillRect(ctx, CGRectMake(right - (right - left)/8.0, top, (right - left)/8.0, bottom - top));
            break;
        case 0x2596: // ▖ Quadrant lower left
            CGContextFillRect(ctx, CGRectMake(left, top, midX - left, midY - top));
            break;
        case 0x2597: // ▗ Quadrant lower right
            CGContextFillRect(ctx, CGRectMake(midX, top, right - midX, midY - top));
            break;
        case 0x2598: // ▘ Quadrant upper left
            CGContextFillRect(ctx, CGRectMake(left, midY, midX - left, bottom - midY));
            break;
        case 0x2599: // ▙ Quadrant upper left + lower left + lower right
            CGContextFillRect(ctx, CGRectMake(left, top, midX - left, midY - top));
            CGContextFillRect(ctx, CGRectMake(midX, top, right - midX, midY - top));
            CGContextFillRect(ctx, CGRectMake(left, midY, midX - left, bottom - midY));
            break;
        case 0x259A: // ▚ Quadrant upper left + lower right
            CGContextFillRect(ctx, CGRectMake(left, midY, midX - left, bottom - midY));
            CGContextFillRect(ctx, CGRectMake(midX, top, right - midX, midY - top));
            break;
        case 0x259B: // ▛ Quadrant upper left + upper right + lower left
            CGContextFillRect(ctx, CGRectMake(left, top, midX - left, midY - top));
            CGContextFillRect(ctx, CGRectMake(left, midY, midX - left, bottom - midY));
            CGContextFillRect(ctx, CGRectMake(midX, midY, right - midX, bottom - midY));
            break;
        case 0x259C: // ▜ Quadrant upper left + upper right + lower right
            CGContextFillRect(ctx, CGRectMake(midX, top, right - midX, midY - top));
            CGContextFillRect(ctx, CGRectMake(left, midY, midX - left, bottom - midY));
            CGContextFillRect(ctx, CGRectMake(midX, midY, right - midX, bottom - midY));
            break;
        case 0x259D: // ▝ Quadrant upper right
            CGContextFillRect(ctx, CGRectMake(midX, midY, right - midX, bottom - midY));
            break;
        case 0x259E: // ▞ Quadrant upper right + lower left
            CGContextFillRect(ctx, CGRectMake(left, top, midX - left, midY - top));
            CGContextFillRect(ctx, CGRectMake(midX, midY, right - midX, bottom - midY));
            break;
        case 0x259F: // ▟ Quadrant upper right + lower left + lower right
            CGContextFillRect(ctx, CGRectMake(midX, top, right - midX, midY - top));
            CGContextFillRect(ctx, CGRectMake(left, top, midX - left, midY - top));
            CGContextFillRect(ctx, CGRectMake(midX, midY, right - midX, bottom - midY));
            break;
        default:
            break;
    }
    CGContextStrokePath(ctx);
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

    // Create atlases for all 4 styles
    for (int style = 0; style < NUM_STYLES; style++) {
        if (!createFontAtlas(fbW, fbH, winW, winH, style)) {
            dbg("createFontAtlas FAILED for style %d\n", style);
            return false;
        }
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_initialized = true;
    dbg("Renderer init OK, cell=%dx%d\n", m_cellWidth, m_cellHeight);
    return true;
}

void Renderer::shutdown() {
    for (int i = 0; i < NUM_STYLES; i++) {
        if (m_fontTexture[i]) { glDeleteTextures(1, &m_fontTexture[i]); m_fontTexture[i] = 0; }
    }
    m_initialized = false;
}

bool Renderer::createFontAtlas(int fbW, int fbH, int winW, int winH, int fontStyle) {
    float scaleFactor = (float)fbW / (float)winW;
    if (scaleFactor < 1.0f) scaleFactor = 1.0f;
    dbg("Scale factor: %.1f\n", scaleFactor);

    const double fontSize = 16.0;
    const double atlasFontSize = fontSize * scaleFactor;
    CTFontRef ctFont = nullptr;

    // Select font based on style
    const char* fontStyleSuffix = "";
    CTFontSymbolicTraits traits = 0;
    switch (fontStyle) {
        case 0: fontStyleSuffix = ""; traits = 0; break; // regular
        case 1: fontStyleSuffix = " Bold"; traits = kCTFontTraitBold; break; // bold
        case 2: fontStyleSuffix = " Italic"; traits = kCTFontTraitItalic; break; // italic
        case 3: fontStyleSuffix = " Bold Italic"; traits = kCTFontTraitBold | kCTFontTraitItalic; break; // bold italic
    }

    for (const char* name : FONT_NAMES) {
        std::string fontNameStr = std::string(name) + fontStyleSuffix;
        CFStringRef fontName = CFStringCreateWithCString(kCFAllocatorDefault, fontNameStr.c_str(), kCFStringEncodingUTF8);
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

    m_cellWidth = (int)round(advance / scaleFactor);
    m_cellHeight = (int)round((ascent + descent + leading) / scaleFactor);
    if (m_cellWidth < 1) m_cellWidth = 8;
    if (m_cellHeight < 1) m_cellHeight = 16;

    int atlasCellW = (int)ceil(advance) + 4;
    int atlasCellH = (int)ceil(ascent + descent + leading) + 4;

    dbg("Cell: %dx%d (logical), atlas: %dx%d (physical, @%.0fpt)\n",
        m_cellWidth, m_cellHeight, atlasCellW, atlasCellH, atlasFontSize);

    // Baseline position in atlas bitmap (CG coords, y=0 at bottom)
    double baselineY = descent + leading / 2.0 + 2.0;

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

    CFStringRef fontAttrKey = kCTFontAttributeName;
    CFDictionaryRef fontAttr = CFDictionaryCreate(
        kCFAllocatorDefault,
        (const void**)&fontAttrKey, (const void**)&ctFont,
        1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    struct RenderedGlyph {
        wchar_t ch;
        CGGlyph glyph;
        CTFontRef font;
        CGFloat adv;
        CGFloat bearingX, bearingY;
        CGFloat bbW, bbH;
        bool isBoxDrawing;
    };
    std::vector<RenderedGlyph> rendered;

    std::vector<wchar_t> missing;
    std::unordered_map<wchar_t, CGGlyph> directGlyphs;

    for (wchar_t ch : chars) {
        UniChar uc = (UniChar)ch;
        CGGlyph glyph = 0;
        CTFontGetGlyphsForCharacters(ctFont, &uc, &glyph, 1);
        if (glyph != 0) {
            directGlyphs[ch] = glyph;
        } else {
            missing.push_back(ch);
        }
    }

    dbg("Direct: %d, missing: %d\n", (int)directGlyphs.size(), (int)missing.size());

    std::unordered_map<wchar_t, std::pair<CGGlyph, CTFontRef>> fallbackGlyphs;
    std::vector<CTFontRef> retainedFonts;
    if (!missing.empty()) {
        std::vector<UniChar> missUni(missing.begin(), missing.end());
        CFStringRef missStr = CFStringCreateWithCharacters(kCFAllocatorDefault, missUni.data(), missUni.size());
        CFAttributedStringRef missAttrStr = CFAttributedStringCreate(kCFAllocatorDefault, missStr, fontAttr);
        CFRelease(missStr);
        CTLineRef missLine = CTLineCreateWithAttributedString(missAttrStr);
        CFRelease(missAttrStr);

        CFArrayRef runs = CTLineGetGlyphRuns(missLine);
        CFIndex runCount = CFArrayGetCount(runs);
        CFIndex charIdx = 0;

        for (CFIndex r = 0; r < runCount && charIdx < (CFIndex)missing.size(); r++) {
            CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, r);
            CFIndex glyphCount = CTRunGetGlyphCount(run);

            CFDictionaryRef runAttrs = CTRunGetAttributes(run);
            CTFontRef runFont = ctFont;
            CFDictionaryGetValueIfPresent(runAttrs, kCTFontAttributeName, (const void**)&runFont);

            if (runFont != ctFont) {
                CFRetain(runFont);
                retainedFonts.push_back(runFont);
            }

            std::vector<CGGlyph> runGlyphs(glyphCount);
            CTRunGetGlyphs(run, CFRangeMake(0, glyphCount), runGlyphs.data());

            for (CFIndex g = 0; g < glyphCount && charIdx < (CFIndex)missing.size(); g++, charIdx++) {
                if (runGlyphs[g] != 0) {
                    fallbackGlyphs[missing[charIdx]] = {runGlyphs[g], runFont};
                }
            }
        }
        CFRelease(missLine);
    }

    dbg("Fallback: %d glyphs found\n", (int)fallbackGlyphs.size());

    for (wchar_t ch : chars) {
        CGGlyph glyph = 0;
        CTFontRef useFont = ctFont;

        auto itDirect = directGlyphs.find(ch);
        if (itDirect != directGlyphs.end()) {
            glyph = itDirect->second;
        } else {
            auto itFallback = fallbackGlyphs.find(ch);
            if (itFallback != fallbackGlyphs.end()) {
                glyph = itFallback->second.first;
                useFont = itFallback->second.second;
            }
        }

        if (glyph == 0) continue;

        CGRect bbRect;
        CTFontGetBoundingRectsForGlyphs(useFont, kCTFontOrientationDefault, &glyph, &bbRect, 1);
        CGFloat advVal = CTFontGetAdvancesForGlyphs(useFont, kCTFontOrientationDefault, &glyph, nullptr, 1);

        bool isBox = (ch >= 0x2500 && ch <= 0x259F);

        RenderedGlyph rg;
        rg.ch = ch;
        rg.glyph = glyph;
        rg.font = useFont;
        rg.adv = advVal;
        rg.bearingX = bbRect.origin.x;
        rg.bearingY = bbRect.origin.y;
        rg.bbW = bbRect.size.width;
        rg.bbH = bbRect.size.height;
        rg.isBoxDrawing = isBox;
        rendered.push_back(rg);
    }

    CFRelease(fontAttr);

    int totalChars = (int)rendered.size();
    int rows = (totalChars + ATLAS_COLS - 1) / ATLAS_COLS;

    m_atlasTexW = ATLAS_COLS * atlasCellW;
    m_atlasTexH = rows * atlasCellH;

    auto nextPow2 = [](int v) { int p = 1; while (p < v) p <<= 1; return p; };
    m_atlasTexW = nextPow2(m_atlasTexW);
    m_atlasTexH = nextPow2(m_atlasTexH);

    dbg("Atlas: %d rendered chars, grid=%dx%d, tex=%dx%d\n", totalChars, ATLAS_COLS, rows, m_atlasTexW, m_atlasTexH);

    std::vector<uint8_t> rgba(m_atlasTexW * m_atlasTexH * 4, 0);

    for (int i = 0; i < totalChars; i++) {
        const RenderedGlyph& rg = rendered[i];
        int col = i % ATLAS_COLS;
        int row = i / ATLAS_COLS;
        int px = col * atlasCellW;
        int py = row * atlasCellH;

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

            if (rg.isBoxDrawing) {
                drawBoxDrawing(ctx, rg.ch, gw, gh, baselineY, ascent, descent, rg.adv);
            } else {
                CGFloat gx = (CGFloat)((gw - rg.adv) / 2.0);
                CGFloat gy = (CGFloat)baselineY;
                CGPoint point = { gx, gy };
                CTFontDrawGlyphs(rg.font, &rg.glyph, &point, 1, ctx);
            }

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

        // Compute glyph bounds in atlas pixel coords
        double renderX = (gw - rg.adv) / 2.0;
        double glyphLeft = renderX + rg.bearingX;
        double glyphTop = baselineY + rg.bearingY;
        double glyphBottom = glyphTop - rg.bbH;
        double glyphRight = glyphLeft + rg.bbW;

        // Clamp to atlas cell bounds
        if (glyphLeft < 0) glyphLeft = 0;
        if (glyphBottom < 0) glyphBottom = 0;
        if (glyphRight > gw) glyphRight = gw;
        if (glyphTop > gh) glyphTop = gh;

        GlyphInfo gi = {};
        gi.u0 = (float)(px + glyphLeft) / m_atlasTexW;
        gi.v0 = (float)(py + glyphTop) / m_atlasTexH;
        gi.u1 = (float)(px + glyphRight) / m_atlasTexW;
        gi.v1 = (float)(py + glyphBottom) / m_atlasTexH;
        gi.width = (float)rg.bbW / scaleFactor;
        gi.height = (float)rg.bbH / scaleFactor;
        gi.bearX = (float)(renderX + rg.bearingX) / scaleFactor;
        gi.bearY = (float)(rg.bearingY) / scaleFactor;
        gi.glyphW = (float)rg.bbW / scaleFactor;
        gi.glyphH = (float)rg.bbH / scaleFactor;
        m_glyphs[fontStyle][rg.ch] = gi;
    }

    CFRelease(ctFont);
    for (CTFontRef f : retainedFonts) CFRelease(f);

    dbg("Font atlas created, glyphs=%d for style %d\n", (int)m_glyphs[fontStyle].size(), fontStyle);

    glGenTextures(1, &m_fontTexture[fontStyle]);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture[fontStyle]);
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

void Renderer::drawCell(int x, int y, wchar_t ch, uint32_t fg, uint32_t bg, bool bold, bool italic) {
    float px = (float)(x * m_cellWidth);
    float py = (float)(y * m_cellHeight);
    float w = (float)m_cellWidth;
    float h = (float)m_cellHeight;

    m_bgBatch.push_back({px, py, w, h, bg});

    if (ch == L' ' || ch == 0) return;

    // Select font style: 0=regular, 1=bold, 2=italic, 3=bold+italic
    int style = 0;
    if (bold && italic) style = 3;
    else if (bold) style = 1;
    else if (italic) style = 2;

    auto it = m_glyphs[style].find(ch);
    if (it == m_glyphs[style].end()) {
        // Fallback to regular style
        it = m_glyphs[0].find(ch);
        style = 0;
        if (it == m_glyphs[0].end()) return;
    }
    const GlyphInfo& gi = it->second;

    // Position glyph using bearing offsets (Alacritty-style)
    float gx = px + gi.bearX;
    float gy = py + (float)m_cellHeight - gi.bearY - gi.glyphH;
    float gw = gi.glyphW;
    float gh = gi.glyphH;

    m_glyphBatch.push_back({gx, gy, gw, gh,
                            gi.u0, gi.v0, gi.u1, gi.v1, fg, style});
}

void Renderer::flushBgBatch() {
    if (m_bgBatch.empty()) return;
    glDisable(GL_TEXTURE_2D);

    std::vector<float> vertices;
    std::vector<uint8_t> colors;
    vertices.reserve(m_bgBatch.size() * 8);
    colors.reserve(m_bgBatch.size() * 16);

    for (const auto& q : m_bgBatch) {
        vertices.push_back(q.x);      vertices.push_back(q.y);
        vertices.push_back(q.x + q.w); vertices.push_back(q.y);
        vertices.push_back(q.x + q.w); vertices.push_back(q.y + q.h);
        vertices.push_back(q.x);      vertices.push_back(q.y + q.h);
        uint8_t r = (q.color >> 16) & 0xFF;
        uint8_t g = (q.color >> 8) & 0xFF;
        uint8_t b = q.color & 0xFF;
        for (int i = 0; i < 4; i++) {
            colors.push_back(r); colors.push_back(g); colors.push_back(b); colors.push_back(255);
        }
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices.data());
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors.data());
    glDrawArrays(GL_QUADS, 0, (GLsizei)(m_bgBatch.size() * 4));
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void Renderer::flushGlyphBatch() {
    if (m_glyphBatch.empty()) return;
    glEnable(GL_TEXTURE_2D);

    int lastStyle = -1;
    int start = 0;

    auto flushRange = [&](int s, int e) {
        if (s >= e) return;
        int count = e - s;
        std::vector<float> vertices;
        std::vector<float> texcoords;
        std::vector<uint8_t> colors;
        vertices.reserve(count * 8);
        texcoords.reserve(count * 8);
        colors.reserve(count * 16);

        for (int i = s; i < e; i++) {
            const auto& q = m_glyphBatch[i];
            vertices.push_back(q.x);      vertices.push_back(q.y);
            vertices.push_back(q.x + q.w); vertices.push_back(q.y);
            vertices.push_back(q.x + q.w); vertices.push_back(q.y + q.h);
            vertices.push_back(q.x);      vertices.push_back(q.y + q.h);
            texcoords.push_back(q.u0); texcoords.push_back(q.v0);
            texcoords.push_back(q.u1); texcoords.push_back(q.v0);
            texcoords.push_back(q.u1); texcoords.push_back(q.v1);
            texcoords.push_back(q.u0); texcoords.push_back(q.v1);
            uint8_t r = (q.color >> 16) & 0xFF;
            uint8_t g = (q.color >> 8) & 0xFF;
            uint8_t b = q.color & 0xFF;
            for (int j = 0; j < 4; j++) {
                colors.push_back(r); colors.push_back(g); colors.push_back(b); colors.push_back(255);
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_fontTexture[m_glyphBatch[s].style]);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, vertices.data());
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords.data());
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors.data());
        glDrawArrays(GL_QUADS, 0, (GLsizei)count * 4);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    };

    for (int i = 0; i < (int)m_glyphBatch.size(); i++) {
        if (m_glyphBatch[i].style != lastStyle) {
            flushRange(start, i);
            start = i;
            lastStyle = m_glyphBatch[i].style;
        }
    }
    flushRange(start, (int)m_glyphBatch.size());
}

void Renderer::drawCursor(int x, int y, int cellW, int cellH, uint32_t color) {
    float px = (float)(x * cellW);
    float py = (float)(y * cellH);
    float w = (float)cellW;
    float h = (float)cellH;

    glDisable(GL_TEXTURE_2D);
    float vertices[] = {
        px, py, px + w, py, px + w, py + h, px, py + h
    };
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint8_t colors[] = {
        r, g, b, 255, r, g, b, 255, r, g, b, 255, r, g, b, 255
    };

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}
