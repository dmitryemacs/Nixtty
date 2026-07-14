#include "pty.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <util.h>

static FILE* pty_log_file = nullptr;

void pty_set_log_file(FILE* f) { pty_log_file = f; }

static void ptylog(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (pty_log_file) { fprintf(pty_log_file, "[PTY] %s", buf); fflush(pty_log_file); }
}

Pty::Pty() = default;
Pty::~Pty() { close(); }

bool Pty::spawn(const std::string& shell) {
    int masterFd = -1;
    int slaveFd = -1;

    if (openpty(&masterFd, &slaveFd, nullptr, nullptr, nullptr) < 0) {
        ptylog("openpty failed: %s\n", strerror(errno));
        return false;
    }

    // Set slave to non-blocking initially for setup, then blocking for reads
    int flags = fcntl(masterFd, F_GETFL, 0);
    fcntl(masterFd, F_SETFL, flags & ~O_NONBLOCK);

    pid_t pid = fork();
    if (pid < 0) {
        ptylog("fork failed: %s\n", strerror(errno));
        ::close(masterFd);
        ::close(slaveFd);
        return false;
    }

    if (pid == 0) {
        // Child process
        ::close(masterFd);

        // Create new session and set controlling terminal
        setsid();
        ioctl(slaveFd, TIOCSCTTY, 0);

        // Redirect stdio to the slave pty
        dup2(slaveFd, STDIN_FILENO);
        dup2(slaveFd, STDOUT_FILENO);
        dup2(slaveFd, STDERR_FILENO);
        if (slaveFd > STDERR_FILENO) ::close(slaveFd);

        // Set initial window size
        struct winsize ws;
        ws.ws_col = m_cols;
        ws.ws_row = m_rows;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl(STDOUT_FILENO, TIOCSWINSZ, &ws);

        // Determine shell
        const char* shellPath = nullptr;
        if (!shell.empty()) {
            shellPath = shell.c_str();
        } else {
            shellPath = getenv("SHELL");
            if (!shellPath) shellPath = "/bin/zsh";
        }

        // Reset signal handlers
        signal(SIGCHLD, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGALRM, SIG_DFL);

        execlp(shellPath, shellPath, nullptr);

        // Fallback to /bin/sh
        execlp("/bin/sh", "/bin/sh", nullptr);

        ptylog("execlp failed: %s\n", strerror(errno));
        _exit(1);
    }

    // Parent process
    ::close(slaveFd);

    m_masterFd = masterFd;
    m_childPid = pid;
    m_running = true;

    m_readThread = std::thread(&Pty::readThread, this);

    ptylog("Shell spawned OK, PID=%d\n", pid);
    return true;
}

void Pty::readThread() {
    ptylog("ReadThread started, fd=%d, running=%d\n", m_masterFd, (int)m_running.load());
    char buffer[4096];
    while (m_running) {
        ssize_t bytesRead = read(m_masterFd, buffer, sizeof(buffer));

        if (bytesRead < 0) {
            if (errno == EINTR) continue;
            ptylog("read error: %s\n", strerror(errno));
            break;
        }

        if (bytesRead == 0) {
            ptylog("read EOF (master closed)\n");
            break;
        }

        if (onData && bytesRead > 0) {
            onData(buffer, bytesRead);
        }
    }
    m_running = false;
    ptylog("ReadThread exit\n");
    if (onExit) onExit();
}

void Pty::resize(int cols, int rows) {
    if (m_masterFd < 0) return;
    m_cols = cols;
    m_rows = rows;
    struct winsize ws;
    ws.ws_col = cols;
    ws.ws_row = rows;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ioctl(m_masterFd, TIOCSWINSZ, &ws);
}

bool Pty::write(const char* data, size_t len) {
    if (m_masterFd < 0 || !m_running) return false;
    ssize_t written = ::write(m_masterFd, data, len);
    return written == (ssize_t)len;
}

void Pty::close() {
    m_running = false;

    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }

    if (m_childPid > 0) {
        kill(m_childPid, SIGHUP);
        int status;
        waitpid(m_childPid, &status, WNOHANG);
        m_childPid = -1;
    }

    if (m_readThread.joinable()) m_readThread.join();
}
