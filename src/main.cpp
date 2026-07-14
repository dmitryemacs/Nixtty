#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <imm.h>
#include <string>
#include <memory>
#include <cstdio>
#include <vector>
#include <algorithm>

#include "terminal.h"
#include "renderer.h"
#include "pty.h"
#include "ansi.h"

static const wchar_t* CLASS_NAME = L"Nixtty";
static const int DEFAULT_COLS = 100;
static const int DEFAULT_ROWS = 30;

static std::unique_ptr<Terminal> g_terminal;
static std::unique_ptr<Renderer> g_renderer;
static std::unique_ptr<Pty> g_pty;
static std::unique_ptr<AnsiParser> g_ansi;
static CRITICAL_SECTION g_lock;
static HWND g_hwnd = nullptr;
static FILE* g_log = nullptr;
static int g_paintCount = 0;
static wchar_t g_highSurrogate = 0;

struct Selection {
    int startX = -1, startY = -1;
    int endX = -1, endY = -1;
    bool selecting = false;
    bool hasSelection() const { return startX >= 0 && endX >= 0; }
    void clear() { startX = startY = endX = endY = -1; selecting = false; }
    bool isSelected(int x, int y) const {
        if (!hasSelection()) return false;
        int sy = startY, ey = endY;
        if (sy > ey) { std::swap(sy, ey); }
        if (y < sy || y > ey) return false;
        if (y == sy && y == ey) {
            int sx = startX, ex = endX;
            if (sx > ex) std::swap(sx, ex);
            return x >= sx && x <= ex;
        }
        if (y == sy) return x >= (startY <= endY ? startX : endX);
        if (y == ey) return x <= (startY <= endY ? endX : startX);
        return true;
    }
};
static Selection g_sel;

static void log(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
    if (g_log) { fprintf(g_log, "%s", buf); fflush(g_log); }
}

static void writeUtf8(wchar_t ch) {
    if (ch >= 0xD800 && ch <= 0xDBFF) {
        g_highSurrogate = ch;
        return;
    }
    if (ch >= 0xDC00 && ch <= 0xDFFF && g_highSurrogate) {
        uint32_t cp = 0x10000 + ((g_highSurrogate - 0xD800) << 10) + (ch - 0xDC00);
        g_highSurrogate = 0;
        char u[5] = {};
        u[0] = (char)(0xF0 | (cp >> 18));
        u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        u[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[3] = (char)(0x80 | (cp & 0x3F));
        g_pty->write(u, 4);
        return;
    }
    g_highSurrogate = 0;
    char u[5] = {};
    size_t len = 0;
    if (ch < 0x80)      { u[0] = (char)ch; len = 1; }
    else if (ch < 0x800){ u[0] = (char)(0xC0|(ch>>6)); u[1] = (char)(0x80|(ch&0x3F)); len = 2; }
    else                { u[0] = (char)(0xE0|(ch>>12)); u[1] = (char)(0x80|((ch>>6)&0x3F)); u[2] = (char)(0x80|(ch&0x3F)); len = 3; }
    g_pty->write(u, len);
}

static void writeUtf16String(const wchar_t* str, int count) {
    for (int i = 0; i < count; i++) {
        writeUtf8(str[i]);
    }
}

static void updateWindowSize() {
    if (!g_hwnd || !g_renderer) return;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    int cellW = g_renderer->getCellWidth();
    int cellH = g_renderer->getCellHeight();

    int cols = width / cellW;
    int rows = height / cellH;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    if (cols != g_terminal->getCols() || rows != g_terminal->getRows()) {
        log("Resize: %dx%d -> %dx%d\n", g_terminal->getCols(), g_terminal->getRows(), cols, rows);
        EnterCriticalSection(&g_lock);
        g_terminal->resize(cols, rows);
        LeaveCriticalSection(&g_lock);
        g_pty->resize(cols, rows);
    }
}

static void sendCharToPty(wchar_t ch) {
    if (ch == 0) {
        char nul = 0;
        g_pty->write(&nul, 1);
        return;
    }
    writeUtf8(ch);
}

static void mouseToCell(int mx, int my, int& cx, int& cy) {
    cx = mx / g_renderer->getCellWidth();
    cy = my / g_renderer->getCellHeight();
    cx = std::clamp(cx, 0, g_terminal->getCols() - 1);
    cy = std::clamp(cy, 0, g_terminal->getRows() - 1);
}

static std::wstring getSelectedText() {
    if (!g_sel.hasSelection()) return {};
    EnterCriticalSection(&g_lock);
    const Cell* buf = g_terminal->getBuffer();
    int cols = g_terminal->getCols();

    int sy = g_sel.startY, ey = g_sel.endY;
    int sx = g_sel.startX, ex = g_sel.endX;
    if (sy > ey || (sy == ey && sx > ex)) {
        std::swap(sx, ex);
        std::swap(sy, ey);
    }

    std::wstring result;
    for (int y = sy; y <= ey; y++) {
        int lineStart = (y == sy) ? sx : 0;
        int lineEnd = (y == ey) ? ex : cols - 1;
        for (int x = lineStart; x <= lineEnd; x++) {
            wchar_t ch = buf[y * cols + x].ch;
            result += ch;
        }
        if (y < ey) result += L'\n';
    }
    LeaveCriticalSection(&g_lock);
    return result;
}

static void copySelectionToClipboard() {
    std::wstring text = getSelectedText();
    if (text.empty()) return;
    if (OpenClipboard(g_hwnd)) {
        EmptyClipboard();
        size_t len = text.size();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t* p = (wchar_t*)GlobalLock(hMem);
            memcpy(p, text.data(), len * sizeof(wchar_t));
            p[len] = 0;
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        updateWindowSize();
        return 0;

    case WM_PAINT: {
        g_paintCount++;
        if (g_paintCount <= 3 || g_paintCount % 60 == 0)
            log("WM_PAINT #%d\n", g_paintCount);

        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (w > 0 && h > 0 && g_renderer && g_renderer->isInitialized() && g_terminal) {
            g_renderer->beginFrame(w, h);

            EnterCriticalSection(&g_lock);
            const Cell* buf = g_terminal->getBuffer();
            int cols = g_terminal->getCols();
            int rows = g_terminal->getRows();
            Cursor cur = g_terminal->getCursor();

            for (int y = 0; y < rows; y++)
                for (int x = 0; x < cols; x++) {
                    uint32_t bg = buf[y * cols + x].bg;
                    if (g_sel.isSelected(x, y))
                        bg = 0x4D6299;
                    g_renderer->drawCell(x, y, buf[y * cols + x].ch, buf[y * cols + x].fg, bg, buf[y * cols + x].bold);
                }
            LeaveCriticalSection(&g_lock);

            g_renderer->flushBatches();

            if (cur.visible && ((GetTickCount() / 500) & 1) == 0)
                g_renderer->drawCursor(cur.x, cur.y, g_renderer->getCellWidth(), g_renderer->getCellHeight(), 0xFFFFFF);

            g_renderer->present();
        } else {
            log("WM_PAINT skip: w=%d h=%d rend=%d init=%d term=%d\n",
                w, h, !!g_renderer, g_renderer ? g_renderer->isInitialized() : 0, !!g_terminal);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN: {
        if (!g_pty) return 0;

        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        bool altGr = ctrl && alt;

        char seqBuf[16];
        const char* seq = nullptr;
        size_t len = 0;

        int mods = (shift ? 1 : 0) | (alt ? 2 : 0) | (ctrl ? 4 : 0);
        int finalMod = 1 + mods;

        if (ctrl && !alt && wParam == 'C' && g_sel.hasSelection()) {
            copySelectionToClipboard();
            g_sel.clear();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (ctrl && !alt && wParam == 'A') {
            g_sel.startX = 0;
            g_sel.startY = 0;
            g_sel.endX = g_terminal->getCols() - 1;
            g_sel.endY = g_terminal->getRows() - 1;
            g_sel.selecting = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (ctrl && !alt && wParam >= 'A' && wParam <= 'Z') {
            char c = (char)(wParam - 'A' + 1);
            g_pty->write(&c, 1);
            return 0;
        }
        if (ctrl && !alt && wParam >= 'a' && wParam <= 'z') {
            char c = (char)(wParam - 'a' + 1);
            g_pty->write(&c, 1);
            return 0;
        }
        if (ctrl && !alt && wParam == VK_SPACE) {
            char nul = 0;
            g_pty->write(&nul, 1);
            return 0;
        }

        if (wParam >= VK_F1 && wParam <= VK_F12) {
            int fi = wParam - VK_F1;
            if (fi < 4) {
                static const char* fbase[] = { "\x1bOP", "\x1bOQ", "\x1bOR", "\x1bOS" };
                if (mods == 0) {
                    seq = fbase[fi]; len = 3;
                } else {
                    int n = snprintf(seqBuf, sizeof(seqBuf), "\x1b[1;%d%c", finalMod, 'P' + fi);
                    seq = seqBuf; len = n;
                }
            } else {
                static const int fnums[] = { 15, 17, 18, 19, 20, 21, 23, 24 };
                if (mods == 0) {
                    int n = snprintf(seqBuf, sizeof(seqBuf), "\x1b[%d~", fnums[fi - 4]);
                    seq = seqBuf; len = n;
                } else {
                    int n = snprintf(seqBuf, sizeof(seqBuf), "\x1b[%d;%d~", fnums[fi - 4], finalMod);
                    seq = seqBuf; len = n;
                }
            }
        }

        if (!seq) {
            bool ext = (lParam >> 24) & 1;
            if (!ext) {
                if (ctrl && !alt && !shift) {
                    switch (wParam) {
                    case VK_LEFT:   seq = "\x1b[1;5D"; len = 6; break;
                    case VK_RIGHT:  seq = "\x1b[1;5C"; len = 6; break;
                    case VK_UP:     seq = "\x1b[1;5A"; len = 6; break;
                    case VK_DOWN:   seq = "\x1b[1;5B"; len = 6; break;
                    case VK_HOME:   seq = "\x1b[1;5H"; len = 6; break;
                    case VK_END:    seq = "\x1b[1;5F"; len = 6; break;
                    case VK_PRIOR:  seq = "\x1b[5;5~"; len = 6; break;
                    case VK_NEXT:   seq = "\x1b[6;5~"; len = 6; break;
                    }
                } else if (ctrl && shift) {
                    switch (wParam) {
                    case VK_LEFT:   seq = "\x1b[1;6D"; len = 6; break;
                    case VK_RIGHT:  seq = "\x1b[1;6C"; len = 6; break;
                    case VK_UP:     seq = "\x1b[1;6A"; len = 6; break;
                    case VK_DOWN:   seq = "\x1b[1;6B"; len = 6; break;
                    case VK_HOME:   seq = "\x1b[1;6H"; len = 6; break;
                    case VK_END:    seq = "\x1b[1;6F"; len = 6; break;
                    }
                } else if (shift && !ctrl && !alt) {
                    switch (wParam) {
                    case VK_LEFT:   seq = "\x1b[1;2D"; len = 6; break;
                    case VK_RIGHT:  seq = "\x1b[1;2C"; len = 6; break;
                    case VK_UP:     seq = "\x1b[1;2A"; len = 6; break;
                    case VK_DOWN:   seq = "\x1b[1;2B"; len = 6; break;
                    case VK_HOME:   seq = "\x1b[1;2H"; len = 6; break;
                    case VK_END:    seq = "\x1b[1;2F"; len = 6; break;
                    case VK_DELETE: seq = "\x1b[3;2~"; len = 6; break;
                    case VK_PRIOR:  seq = "\x1b[5;2~"; len = 6; break;
                    case VK_NEXT:   seq = "\x1b[6;2~"; len = 6; break;
                    }
                } else if (!shift && !ctrl && !alt) {
                    switch (wParam) {
                    case VK_LEFT:   seq = "\x1b[D"; len = 3; break;
                    case VK_RIGHT:  seq = "\x1b[C"; len = 3; break;
                    case VK_UP:     seq = "\x1b[A"; len = 3; break;
                    case VK_DOWN:   seq = "\x1b[B"; len = 3; break;
                    case VK_HOME:   seq = "\x1b[H"; len = 3; break;
                    case VK_END:    seq = "\x1b[F"; len = 3; break;
                    case VK_DELETE: seq = "\x1b[3~"; len = 4; break;
                    case VK_PRIOR:  seq = "\x1b[5~"; len = 4; break;
                    case VK_NEXT:   seq = "\x1b[6~"; len = 4; break;
                    case VK_INSERT: seq = "\x1b[2~"; len = 4; break;
                    case VK_RETURN: g_pty->write("\r", 1); return 0;
                    case VK_BACK:   g_pty->write("\b", 1); return 0;
                    case VK_TAB:    g_pty->write("\t", 1); return 0;
                    case VK_ESCAPE: g_pty->write("\x1b", 1); return 0;
                    }
                } else if (ctrl && alt) {
                    switch (wParam) {
                    case VK_LEFT:   seq = "\x1b[1;7D"; len = 6; break;
                    case VK_RIGHT:  seq = "\x1b[1;7C"; len = 6; break;
                    case VK_UP:     seq = "\x1b[1;7A"; len = 6; break;
                    case VK_DOWN:   seq = "\x1b[1;7B"; len = 6; break;
                    case VK_HOME:   seq = "\x1b[1;7H"; len = 6; break;
                    case VK_END:    seq = "\x1b[1;7F"; len = 6; break;
                    }
                }
            }
        }

        if (seq) {
            g_pty->write(seq, len);
            return 0;
        }

        BYTE keyState[256];
        GetKeyboardState(keyState);
        if (altGr) {
            keyState[VK_CONTROL] &= 0x7F;
        }

        wchar_t wbuf[8] = {};
        int charCount = ToUnicode(wParam, (lParam >> 16) & 0xFF, keyState, wbuf, 8, 0);

        if (charCount > 0) {
            if (alt && !altGr) {
                char esc = '\x1b';
                g_pty->write(&esc, 1);
            }
            writeUtf16String(wbuf, charCount);
        }

        return 0;
    }

    case WM_SYSKEYDOWN: {
        if (!g_pty) return 0;

        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (ctrl) return DefWindowProcW(hwnd, msg, wParam, lParam);

        BYTE keyState[256];
        GetKeyboardState(keyState);
        wchar_t wbuf[8] = {};
        int charCount = ToUnicode(wParam, (lParam >> 16) & 0xFF, keyState, wbuf, 8, 0);

        if (charCount > 0) {
            char esc = '\x1b';
            g_pty->write(&esc, 1);
            writeUtf16String(wbuf, charCount);
        } else {
            switch (wParam) {
            case VK_F1:  g_pty->write("\x1bOP", 3); break;
            case VK_F2:  g_pty->write("\x1bOQ", 3); break;
            case VK_F3:  g_pty->write("\x1bOR", 3); break;
            case VK_F4:  g_pty->write("\x1bOS", 3); break;
            case VK_LEFT:  g_pty->write("\x1b[1;3D", 6); break;
            case VK_RIGHT: g_pty->write("\x1b[1;3C", 6); break;
            case VK_UP:    g_pty->write("\x1b[1;3A", 6); break;
            case VK_DOWN:  g_pty->write("\x1b[1;3B", 6); break;
            case VK_HOME:  g_pty->write("\x1b[1;3H", 6); break;
            case VK_END:   g_pty->write("\x1b[1;3F", 6); break;
            case VK_PRIOR: g_pty->write("\x1b[5;3~", 6); break;
            case VK_NEXT:  g_pty->write("\x1b[6;3~", 6); break;
            }
        }

        return 0;
    }

    case WM_IME_CHAR: {
        if (!g_pty) return 0;
        wchar_t ch = (wchar_t)wParam;
        writeUtf8(ch);
        return 0;
    }

    case WM_IME_COMPOSITION: {
        if (!g_pty) return 0;
        if (lParam & GCS_RESULTSTR) {
            HIMC hIMC = ImmGetContext(hwnd);
            if (hIMC) {
                LONG len = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, nullptr, 0);
                if (len > 0) {
                    std::vector<wchar_t> buf(len / sizeof(wchar_t));
                    ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buf.data(), len);
                    writeUtf16String(buf.data(), (int)buf.size());
                }
                ImmReleaseContext(hwnd, hIMC);
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (!g_renderer || !g_terminal) return 0;
        SetCapture(hwnd);
        g_sel.clear();
        mouseToCell(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), g_sel.startX, g_sel.startY);
        g_sel.endX = g_sel.startX;
        g_sel.endY = g_sel.startY;
        g_sel.selecting = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!g_sel.selecting || !g_renderer || !g_terminal) return 0;
        int cx, cy;
        mouseToCell(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), cx, cy);
        if (cx != g_sel.endX || cy != g_sel.endY) {
            g_sel.endX = cx;
            g_sel.endY = cy;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (!g_sel.selecting) return 0;
        g_sel.selecting = false;
        ReleaseCapture();
        if (g_sel.startX == g_sel.endX && g_sel.startY == g_sel.endY)
            g_sel.clear();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (!g_pty) return 0;
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        const char* seq = (delta > 0) ? "\x1b[5~" : "\x1b[6~";
        int count = abs(delta / WHEEL_DELTA);
        if (count == 0) count = 1;
        for (int i = 0; i < count; i++) g_pty->write(seq, 4);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        int cw = g_renderer ? g_renderer->getCellWidth() : 9;
        int ch = g_renderer ? g_renderer->getCellHeight() : 18;
        mmi->ptMinTrackSize.x = cw * 20;
        mmi->ptMinTrackSize.y = ch * 5;
        return 0;
    }

    case WM_MOUSEACTIVATE:
        SetFocus(hwnd);
        return MA_ACTIVATE;

    case WM_TIMER:
        if (wParam == 1) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    {
        typedef BOOL (WINAPI *PFN_SetProcessDpiAwarenessContext)(HANDLE);
        auto fn = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(
            GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
        if (fn) fn((HANDLE)-4);
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* sl = wcsrchr(exePath, L'\\');
    if (sl) { wcscpy(sl + 1, L"nixtty.log"); g_log = _wfopen(exePath, L"w"); }

    log("=== START ===\n");
    pty_set_log_file(g_log);
    renderer_set_log_file(g_log);

    InitializeCriticalSection(&g_lock);
    g_terminal = std::make_unique<Terminal>(DEFAULT_COLS, DEFAULT_ROWS);
    g_renderer = std::make_unique<Renderer>();
    g_pty = std::make_unique<Pty>();
    g_ansi = std::make_unique<AnsiParser>(*g_terminal);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_IBEAM);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, CLASS_NAME, L"Nixtty", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInstance, nullptr);
    if (!g_hwnd) { log("CreateWindow failed\n"); return 1; }

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwnd, 20, &dark, sizeof(dark));

    if (!g_renderer->init(g_hwnd)) {
        log("RENDERER INIT FAILED\n");
        MessageBoxW(g_hwnd, L"OpenGL init failed", L"Nixtty", MB_ICONERROR);
        return 1;
    }

    int cw = g_renderer->getCellWidth();
    int ch = g_renderer->getCellHeight();
    int winW = DEFAULT_COLS * cw + 16;
    int winH = DEFAULT_ROWS * ch + 39;
    SetWindowPos(g_hwnd, nullptr, 0, 0, winW, winH, SWP_NOMOVE | SWP_NOZORDER);

    log("Setting up PTY callbacks\n");
    g_pty->onData = [](const char* data, size_t len) {
        log("onData: %d bytes\n", (int)len);
        EnterCriticalSection(&g_lock);
        g_ansi->parse(data, len);
        LeaveCriticalSection(&g_lock);
        InvalidateRect(g_hwnd, nullptr, FALSE);
    };
    g_pty->onExit = []() { PostMessage(g_hwnd, WM_CLOSE, 0, 0); };

    g_ansi->onWrite = [](const char* data, size_t len) {
        g_pty->write(data, len);
    };

    g_terminal->onWrite = [](const char* data, size_t len) {
        g_pty->write(data, len);
    };

    log("Spawning shell\n");
    if (!g_pty->spawn()) {
        log("PTY SPAWN FAILED\n");
        MessageBoxW(g_hwnd, L"Shell spawn failed", L"Nixtty", MB_ICONERROR);
        return 1;
    }

    updateWindowSize();
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    SetTimer(g_hwnd, 1, 500, nullptr);

    log("Entering message loop\n");

    MSG msg;
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        WaitMessage();
    }

done:
    log("Shutting down\n");
    g_pty->close();
    g_renderer->shutdown();
    DeleteCriticalSection(&g_lock);
    log("=== END (paints=%d) ===\n", g_paintCount);
    if (g_log) fclose(g_log);
    return 0;
}
