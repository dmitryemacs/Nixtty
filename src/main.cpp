#include <GLFW/glfw3.h>
#include <string>
#include <memory>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <mutex>

#include "terminal.h"
#include "renderer.h"
#include "pty.h"
#include "ansi.h"

static const int DEFAULT_COLS = 80;
static const int DEFAULT_ROWS = 24;

static std::unique_ptr<Terminal> g_terminal;
static std::unique_ptr<Renderer> g_renderer;
static std::unique_ptr<Pty> g_pty;
static std::unique_ptr<AnsiParser> g_ansi;
static std::mutex g_lock;
static GLFWwindow* g_window = nullptr;
static FILE* g_log = nullptr;
static int g_paintCount = 0;

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
    if (g_log) { fprintf(g_log, "%s", buf); fflush(g_log); }
}

static void writeUtf8(unsigned int codepoint) {
    char u[5] = {};
    size_t len = 0;
    if (codepoint < 0x80) {
        u[0] = (char)codepoint; len = 1;
    } else if (codepoint < 0x800) {
        u[0] = (char)(0xC0 | (codepoint >> 6));
        u[1] = (char)(0x80 | (codepoint & 0x3F));
        len = 2;
    } else if (codepoint < 0x10000) {
        u[0] = (char)(0xE0 | (codepoint >> 12));
        u[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        u[2] = (char)(0x80 | (codepoint & 0x3F));
        len = 3;
    } else {
        u[0] = (char)(0xF0 | (codepoint >> 18));
        u[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        u[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        u[3] = (char)(0x80 | (codepoint & 0x3F));
        len = 4;
    }
    g_pty->write(u, len);
}

static void updateWindowSize() {
    if (!g_window || !g_renderer) return;
    int width, height;
    glfwGetWindowSize(g_window, &width, &height);
    if (width <= 0 || height <= 0) return;

    int cellW = g_renderer->getCellWidth();
    int cellH = g_renderer->getCellHeight();

    int cols = width / cellW;
    int rows = height / cellH;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    if (cols != g_terminal->getCols() || rows != g_terminal->getRows()) {
        log("Resize: %dx%d -> %dx%d\n", g_terminal->getCols(), g_terminal->getRows(), cols, rows);
        std::lock_guard<std::mutex> lock(g_lock);
        g_terminal->resize(cols, rows);
        g_pty->resize(cols, rows);
    }
}

static void mouseToCell(double mx, double my, int& cx, int& cy) {
    cx = (int)mx / g_renderer->getCellWidth();
    cy = (int)my / g_renderer->getCellHeight();
    cx = std::clamp(cx, 0, g_terminal->getCols() - 1);
    cy = std::clamp(cy, 0, g_terminal->getRows() - 1);
}

static std::string getSelectedText() {
    if (!g_sel.hasSelection()) return {};
    std::lock_guard<std::mutex> lock(g_lock);
    const Cell* buf = g_terminal->getBuffer();
    int cols = g_terminal->getCols();

    int sy = g_sel.startY, ey = g_sel.endY;
    int sx = g_sel.startX, ex = g_sel.endX;
    if (sy > ey || (sy == ey && sx > ex)) {
        std::swap(sx, ex);
        std::swap(sy, ey);
    }

    std::string result;
    for (int y = sy; y <= ey; y++) {
        int lineStart = (y == sy) ? sx : 0;
        int lineEnd = (y == ey) ? ex : cols - 1;
        for (int x = lineStart; x <= lineEnd; x++) {
            wchar_t ch = buf[y * cols + x].ch;
            // Convert wchar_t to UTF-8
            unsigned int cp = (unsigned int)ch;
            if (cp < 0x80) {
                result += (char)cp;
            } else if (cp < 0x800) {
                result += (char)(0xC0 | (cp >> 6));
                result += (char)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                result += (char)(0xE0 | (cp >> 12));
                result += (char)(0x80 | ((cp >> 6) & 0x3F));
                result += (char)(0x80 | (cp & 0x3F));
            }
        }
        if (y < ey) result += '\n';
    }
    return result;
}

static void copySelectionToClipboard() {
    std::string text = getSelectedText();
    if (text.empty()) return;
    glfwSetClipboardString(g_window, text.c_str());
}

// GLFW callbacks

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (!g_pty) return;

    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    bool alt = (mods & GLFW_MOD_ALT) != 0;
    bool super = (mods & GLFW_MOD_SUPER) != 0;

    // Cmd+C -> copy on macOS (Super modifier)
    if (super && key == GLFW_KEY_C && g_sel.hasSelection()) {
        copySelectionToClipboard();
        g_sel.clear();
        return;
    }
    // Cmd+A -> select all
    if (super && key == GLFW_KEY_A) {
        g_sel.startX = 0;
        g_sel.startY = 0;
        g_sel.endX = g_terminal->getCols() - 1;
        g_sel.endY = g_terminal->getRows() - 1;
        g_sel.selecting = false;
        return;
    }
    // Cmd+V -> paste on macOS
    if (super && key == GLFW_KEY_V) {
        const char* clip = glfwGetClipboardString(window);
        if (clip) {
            g_pty->write(clip, strlen(clip));
        }
        return;
    }

    // Ctrl+C -> send SIGINT (0x03)
    if (ctrl && !alt && key == GLFW_KEY_C) {
        char c = 3;
        g_pty->write(&c, 1);
        return;
    }
    // Ctrl+Z -> send SIGTSTP
    if (ctrl && !alt && key == GLFW_KEY_Z) {
        char c = 26;
        g_pty->write(&c, 1);
        return;
    }

    // Ctrl+A through Ctrl+Z
    if (ctrl && !alt && key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        char c = (char)(key - GLFW_KEY_A + 1);
        g_pty->write(&c, 1);
        return;
    }

    int mods_num = (shift ? 1 : 0) | (alt ? 2 : 0) | (ctrl ? 4 : 0);
    int finalMod = 1 + mods_num;

    char seqBuf[16];
    const char* seq = nullptr;
    size_t len = 0;

    // Function keys
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
        int fi = key - GLFW_KEY_F1;
        if (fi < 4) {
            static const char* fbase[] = { "\x1bOP", "\x1bOQ", "\x1bOR", "\x1bOS" };
            if (mods_num == 0) {
                seq = fbase[fi]; len = 3;
            } else {
                int n = snprintf(seqBuf, sizeof(seqBuf), "\x1b[1;%d%c", finalMod, 'P' + fi);
                seq = seqBuf; len = n;
            }
        } else {
            static const int fnums[] = { 15, 17, 18, 19, 20, 21, 23, 24 };
            if (mods_num == 0) {
                int n = snprintf(seqBuf, sizeof(seqBuf), "\x1b[%d~", fnums[fi - 4]);
                seq = seqBuf; len = n;
            } else {
                int n = snprintf(seqBuf, sizeof(seqBuf), "\x1b[%d;%d~", fnums[fi - 4], finalMod);
                seq = seqBuf; len = n;
            }
        }
    }

    if (!seq) {
        if (ctrl && !alt && !shift) {
            switch (key) {
            case GLFW_KEY_LEFT:   seq = "\x1b[1;5D"; len = 6; break;
            case GLFW_KEY_RIGHT:  seq = "\x1b[1;5C"; len = 6; break;
            case GLFW_KEY_UP:     seq = "\x1b[1;5A"; len = 6; break;
            case GLFW_KEY_DOWN:   seq = "\x1b[1;5B"; len = 6; break;
            case GLFW_KEY_HOME:   seq = "\x1b[1;5H"; len = 6; break;
            case GLFW_KEY_END:    seq = "\x1b[1;5F"; len = 6; break;
            case GLFW_KEY_PAGE_UP:   seq = "\x1b[5;5~"; len = 6; break;
            case GLFW_KEY_PAGE_DOWN: seq = "\x1b[6;5~"; len = 6; break;
            }
        } else if (ctrl && shift) {
            switch (key) {
            case GLFW_KEY_LEFT:   seq = "\x1b[1;6D"; len = 6; break;
            case GLFW_KEY_RIGHT:  seq = "\x1b[1;6C"; len = 6; break;
            case GLFW_KEY_UP:     seq = "\x1b[1;6A"; len = 6; break;
            case GLFW_KEY_DOWN:   seq = "\x1b[1;6B"; len = 6; break;
            case GLFW_KEY_HOME:   seq = "\x1b[1;6H"; len = 6; break;
            case GLFW_KEY_END:    seq = "\x1b[1;6F"; len = 6; break;
            }
        } else if (shift && !ctrl && !alt) {
            switch (key) {
            case GLFW_KEY_LEFT:   seq = "\x1b[1;2D"; len = 6; break;
            case GLFW_KEY_RIGHT:  seq = "\x1b[1;2C"; len = 6; break;
            case GLFW_KEY_UP:     seq = "\x1b[1;2A"; len = 6; break;
            case GLFW_KEY_DOWN:   seq = "\x1b[1;2B"; len = 6; break;
            case GLFW_KEY_HOME:   seq = "\x1b[1;2H"; len = 6; break;
            case GLFW_KEY_END:    seq = "\x1b[1;2F"; len = 6; break;
            case GLFW_KEY_DELETE: seq = "\x1b[3;2~"; len = 6; break;
            case GLFW_KEY_PAGE_UP:   seq = "\x1b[5;2~"; len = 6; break;
            case GLFW_KEY_PAGE_DOWN: seq = "\x1b[6;2~"; len = 6; break;
            }
        } else if (!shift && !ctrl && !alt) {
            switch (key) {
            case GLFW_KEY_LEFT:   seq = "\x1b[D"; len = 3; break;
            case GLFW_KEY_RIGHT:  seq = "\x1b[C"; len = 3; break;
            case GLFW_KEY_UP:     seq = "\x1b[A"; len = 3; break;
            case GLFW_KEY_DOWN:   seq = "\x1b[B"; len = 3; break;
            case GLFW_KEY_HOME:   seq = "\x1b[H"; len = 3; break;
            case GLFW_KEY_END:    seq = "\x1b[F"; len = 3; break;
            case GLFW_KEY_DELETE: seq = "\x1b[3~"; len = 4; break;
            case GLFW_KEY_PAGE_UP:   seq = "\x1b[5~"; len = 4; break;
            case GLFW_KEY_PAGE_DOWN: seq = "\x1b[6~"; len = 4; break;
            case GLFW_KEY_INSERT: seq = "\x1b[2~"; len = 4; break;
            case GLFW_KEY_ENTER:  g_pty->write("\r", 1); return;
            case GLFW_KEY_BACKSPACE: g_pty->write("\b", 1); return;
            case GLFW_KEY_TAB:    g_pty->write("\t", 1); return;
            case GLFW_KEY_ESCAPE: g_pty->write("\x1b", 1); return;
            }
        } else if (ctrl && alt) {
            switch (key) {
            case GLFW_KEY_LEFT:   seq = "\x1b[1;7D"; len = 6; break;
            case GLFW_KEY_RIGHT:  seq = "\x1b[1;7C"; len = 6; break;
            case GLFW_KEY_UP:     seq = "\x1b[1;7A"; len = 6; break;
            case GLFW_KEY_DOWN:   seq = "\x1b[1;7B"; len = 6; break;
            case GLFW_KEY_HOME:   seq = "\x1b[1;7H"; len = 6; break;
            case GLFW_KEY_END:    seq = "\x1b[1;7F"; len = 6; break;
            }
        }
    }

    if (seq) {
        g_pty->write(seq, len);
        return;
    }
}

static void charCallback(GLFWwindow* window, unsigned int codepoint) {
    if (!g_pty) return;
    writeUtf8(codepoint);
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (!g_renderer || !g_terminal) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            g_sel.clear();
            mouseToCell(mx, my, g_sel.startX, g_sel.startY);
            g_sel.endX = g_sel.startX;
            g_sel.endY = g_sel.startY;
            g_sel.selecting = true;
        } else if (action == GLFW_RELEASE) {
            if (g_sel.selecting) {
                g_sel.selecting = false;
                if (g_sel.startX == g_sel.endX && g_sel.startY == g_sel.endY)
                    g_sel.clear();
            }
        }
    }
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (!g_sel.selecting || !g_renderer || !g_terminal) return;
    int cx, cy;
    mouseToCell(xpos, ypos, cx, cy);
    if (cx != g_sel.endX || cy != g_sel.endY) {
        g_sel.endX = cx;
        g_sel.endY = cy;
    }
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (!g_pty || !g_terminal) return;
    int count = abs((int)yoffset);
    if (count == 0) count = 1;

    int shiftState = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT);
    int shiftState2 = glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT);
    bool shift = (shiftState == GLFW_PRESS) || (shiftState2 == GLFW_PRESS);

    if (shift) {
        std::lock_guard<std::mutex> lock(g_lock);
        if (yoffset > 0) {
            g_terminal->scrollBack(count);
        } else {
            g_terminal->scrollForward(count);
        }
    } else {
        if (g_terminal->isScrolledBack()) {
            std::lock_guard<std::mutex> lock(g_lock);
            g_terminal->scrollToBottom();
        }
        const char* seq = (yoffset > 0) ? "\x1b[5~" : "\x1b[6~";
        for (int i = 0; i < count; i++) g_pty->write(seq, 4);
    }
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    updateWindowSize();
}

static void windowRefreshCallback(GLFWwindow* window) {
    if (!g_renderer || !g_renderer->isInitialized() || !g_terminal) return;

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    if (fbWidth <= 0 || fbHeight <= 0) return;

    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);

    g_paintCount++;

    g_renderer->beginFrame(fbWidth, fbHeight, winWidth, winHeight);

    {
        std::lock_guard<std::mutex> lock(g_lock);
        int cols = g_terminal->getCols();
        int rows = g_terminal->getRows();
        int scrollOffset = g_terminal->getScrollOffset();
        int scrollbackLines = g_terminal->getScrollbackLines();

        if (scrollOffset > 0 && scrollbackLines > 0) {
            // Scrolled back: show scrollback lines at top, current buffer at bottom
            int visibleScrollback = std::min(scrollOffset, rows);
            int bufferRows = rows - visibleScrollback;

            // Draw scrollback lines
            for (int y = 0; y < visibleScrollback; y++) {
                int srcIdx = scrollbackLines - scrollOffset + y;
                const std::vector<Cell>* line = g_terminal->getScrollbackLine(srcIdx);
                if (!line) continue;
                for (int x = 0; x < cols && x < (int)line->size(); x++) {
                    g_renderer->drawCell(x, y, (*line)[x].ch, (*line)[x].fg, (*line)[x].bg, (*line)[x].bold);
                }
            }

            // Draw current buffer (shifted down)
            const Cell* buf = g_terminal->getBuffer();
            for (int y = 0; y < bufferRows; y++)
                for (int x = 0; x < cols; x++) {
                    int dstY = visibleScrollback + y;
                    g_renderer->drawCell(x, dstY, buf[y * cols + x].ch, buf[y * cols + x].fg, buf[y * cols + x].bg, buf[y * cols + x].bold);
                }
        } else {
            // Normal view
            const Cell* buf = g_terminal->getBuffer();
            for (int y = 0; y < rows; y++)
                for (int x = 0; x < cols; x++) {
                    uint32_t bg = buf[y * cols + x].bg;
                    if (g_sel.isSelected(x, y))
                        bg = 0x4D6299;
                    g_renderer->drawCell(x, y, buf[y * cols + x].ch, buf[y * cols + x].fg, bg, buf[y * cols + x].bold);
                }
        }
    }

    g_renderer->flushBatches();

    // Cursor blink
    double time = glfwGetTime();
    Cursor cur = g_terminal->getCursor();
    if (cur.visible && ((int)(time * 2.0) & 1) == 0)
        g_renderer->drawCursor(cur.x, cur.y, g_renderer->getCellWidth(), g_renderer->getCellHeight(), 0xFFFFFF);

    g_renderer->present();
}

int main(int argc, char* argv[]) {
    // Open log file
    g_log = fopen("nixtty.log", "w");
    log("=== START (macOS) ===\n");

    if (!glfwInit()) {
        log("GLFW init failed\n");
        return 1;
    }

    // Request OpenGL 2.1 (compatible with legacy profile for GL 1.1 calls)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

    g_terminal = std::make_unique<Terminal>(DEFAULT_COLS, DEFAULT_ROWS);
    g_renderer = std::make_unique<Renderer>();
    g_pty = std::make_unique<Pty>();
    g_ansi = std::make_unique<AnsiParser>(*g_terminal);

    g_window = glfwCreateWindow(800, 600, "Nixtty", nullptr, nullptr);
    if (!g_window) {
        log("Window creation failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1); // VSync

    // Set callbacks
    glfwSetKeyCallback(g_window, keyCallback);
    glfwSetCharCallback(g_window, charCallback);
    glfwSetMouseButtonCallback(g_window, mouseButtonCallback);
    glfwSetCursorPosCallback(g_window, cursorPosCallback);
    glfwSetScrollCallback(g_window, scrollCallback);
    glfwSetFramebufferSizeCallback(g_window, framebufferSizeCallback);
    glfwSetWindowRefreshCallback(g_window, windowRefreshCallback);

    renderer_set_log_file(g_log);
    if (!g_renderer->init(g_window)) {
        log("RENDERER INIT FAILED\n");
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return 1;
    }

    // Set initial window size - fill ~70% of screen
    int cw = g_renderer->getCellWidth();
    int ch = g_renderer->getCellHeight();
    int winW, winH;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        int mx, my, mw, mh;
        glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
        winW = (int)(mw * 0.7);
        winH = (int)(mh * 0.7);
    } else {
        winW = DEFAULT_COLS * cw + 16;
        winH = DEFAULT_ROWS * ch + 39;
    }

    glfwSetWindowSize(g_window, winW, winH);
    log("Window: %dx%d, cell: %dx%d, screen: %dx%d\n", winW, winH, cw, ch,
        monitor ? glfwGetVideoMode(monitor)->width : 0,
        monitor ? glfwGetVideoMode(monitor)->height : 0);

    log("Setting up PTY callbacks\n");
    g_pty->onData = [](const char* data, size_t len) {
        log("onData: %d bytes\n", (int)len);
        std::lock_guard<std::mutex> lock(g_lock);
        g_ansi->parse(data, len);
        glfwPostEmptyEvent();
    };
    g_pty->onExit = []() {
        glfwSetWindowShouldClose(g_window, GLFW_TRUE);
    };

    g_ansi->onWrite = [](const char* data, size_t len) {
        g_pty->write(data, len);
    };

    g_terminal->onWrite = [](const char* data, size_t len) {
        g_pty->write(data, len);
    };

    log("Spawning shell\n");
    if (!g_pty->spawn()) {
        log("PTY SPAWN FAILED\n");
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return 1;
    }

    updateWindowSize();
    glfwPostEmptyEvent();

    log("Entering main loop\n");

    // Main loop
    while (!glfwWindowShouldClose(g_window)) {
        glfwWaitEvents();

        // Schedule periodic redraws for cursor blink
        glfwPostEmptyEvent();

        windowRefreshCallback(g_window);
    }

    log("Shutting down\n");
    g_pty->close();
    g_renderer->shutdown();
    glfwDestroyWindow(g_window);
    glfwTerminate();

    log("=== END (paints=%d) ===\n", g_paintCount);
    if (g_log) fclose(g_log);
    return 0;
}
