#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <string>
#include <memory>
#include <cstdio>

#include "terminal.h"
#include "renderer.h"
#include "pty.h"
#include "ansi.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")

static const wchar_t* CLASS_NAME = L"WinTerm";
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

static void log(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
    if (g_log) { fprintf(g_log, "%s", buf); fflush(g_log); }
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
            LeaveCriticalSection(&g_lock);

            int cw = g_renderer->getCellWidth();
            int ch = g_renderer->getCellHeight();

            for (int y = 0; y < rows; y++)
                for (int x = 0; x < cols; x++)
                    g_renderer->drawCell(x, y, buf[y * cols + x].ch, buf[y * cols + x].fg, buf[y * cols + x].bg, buf[y * cols + x].bold);

            if (cur.visible && ((GetTickCount() / 500) & 1) == 0)
                g_renderer->drawCursor(cur.x, cur.y, cw, ch, 0x7AA2F7);

            g_renderer->endFrame();
        } else {
            log("WM_PAINT skip: w=%d h=%d rend=%d init=%d term=%d\n",
                w, h, !!g_renderer, g_renderer ? g_renderer->isInitialized() : 0, !!g_terminal);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CHAR: {
        if (!g_pty) return 0;
        wchar_t ch = (wchar_t)wParam;
        if (ch == VK_RETURN)       { g_pty->write("\r", 1); }
        else if (ch == VK_BACK)    { g_pty->write("\b", 1); }
        else if (ch == VK_TAB)     { g_pty->write("\t", 1); }
        else if (ch == VK_ESCAPE)  { g_pty->write("\x1b", 1); }
        else if (ch < 32 && ch != 0) {
            // Control characters (Ctrl+A=1, Ctrl+C=3, etc.)
            char ctrl = (char)ch;
            g_pty->write(&ctrl, 1);
        }
        else if (ch >= 32) {
            char u[5] = {};
            size_t len = 0;
            if (ch < 0x80)      { u[0] = (char)ch; len = 1; }
            else if (ch < 0x800){ u[0] = 0xC0|(ch>>6); u[1] = 0x80|(ch&0x3F); len = 2; }
            else                { u[0] = 0xE0|(ch>>12); u[1] = 0x80|((ch>>6)&0x3F); u[2] = 0x80|(ch&0x3F); len = 3; }
            g_pty->write(u, len);
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (!g_pty) return 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        const char* seq = nullptr;
        size_t len = 0;
        if (ctrl && !alt) {
            switch (wParam) {
            case VK_UP:     seq = "\x1b[1;5A"; len = 6; break;
            case VK_DOWN:   seq = "\x1b[1;5B"; len = 6; break;
            case VK_RIGHT:  seq = "\x1b[1;5C"; len = 6; break;
            case VK_LEFT:   seq = "\x1b[1;5D"; len = 6; break;
            case VK_HOME:   seq = "\x1b[1;5H"; len = 6; break;
            case VK_END:    seq = "\x1b[1;5F"; len = 6; break;
            }
        } else if (shift && !ctrl && !alt) {
            switch (wParam) {
            case VK_UP:     seq = "\x1b[1;2A"; len = 6; break;
            case VK_DOWN:   seq = "\x1b[1;2B"; len = 6; break;
            case VK_RIGHT:  seq = "\x1b[1;2C"; len = 6; break;
            case VK_LEFT:   seq = "\x1b[1;2D"; len = 6; break;
            case VK_HOME:   seq = "\x1b[1;2H"; len = 6; break;
            case VK_END:    seq = "\x1b[1;2F"; len = 6; break;
            case VK_DELETE: seq = "\x1b[3;2~"; len = 6; break;
            case VK_PRIOR:  seq = "\x1b[5;2~"; len = 6; break;
            case VK_NEXT:   seq = "\x1b[6;2~"; len = 6; break;
            }
        } else if (!shift && !ctrl && !alt) {
            switch (wParam) {
            case VK_UP:     seq = "\x1b[A"; len = 3; break;
            case VK_DOWN:   seq = "\x1b[B"; len = 3; break;
            case VK_RIGHT:  seq = "\x1b[C"; len = 3; break;
            case VK_LEFT:   seq = "\x1b[D"; len = 3; break;
            case VK_HOME:   seq = "\x1b[H"; len = 3; break;
            case VK_END:    seq = "\x1b[F"; len = 3; break;
            case VK_DELETE: seq = "\x1b[3~"; len = 4; break;
            case VK_PRIOR:  seq = "\x1b[5~"; len = 4; break;
            case VK_NEXT:   seq = "\x1b[6~"; len = 4; break;
            case VK_INSERT: seq = "\x1b[2~"; len = 4; break;
            }
        }
        if (seq) { g_pty->write(seq, len); return 0; }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
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

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* sl = wcsrchr(exePath, L'\\');
    if (sl) { wcscpy(sl + 1, L"winterm.log"); g_log = _wfopen(exePath, L"w"); }

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

    g_hwnd = CreateWindowExW(0, CLASS_NAME, L"WinTerm", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInstance, nullptr);
    if (!g_hwnd) { log("CreateWindow failed\n"); return 1; }

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwnd, 20, &dark, sizeof(dark));

    if (!g_renderer->init(g_hwnd)) {
        log("RENDERER INIT FAILED\n");
        MessageBoxW(g_hwnd, L"OpenGL init failed", L"WinTerm", MB_ICONERROR);
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

    log("Spawning shell\n");
    if (!g_pty->spawn()) {
        log("PTY SPAWN FAILED\n");
        MessageBoxW(g_hwnd, L"Shell spawn failed", L"WinTerm", MB_ICONERROR);
        return 1;
    }

    updateWindowSize();
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    log("Entering message loop\n");

    MSG msg;
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(1);
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
