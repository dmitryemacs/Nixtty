#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

#ifndef HPCON_DEFINED
#define HPCON_DEFINED
typedef VOID* HPCON;
#endif

extern "C" {
    HRESULT WINAPI CreatePseudoConsole(
        COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC
    );
    HRESULT WINAPI ResizePseudoConsole(HPCON hPC, COORD size);
    VOID WINAPI ClosePseudoConsole(HPCON hPC);
}

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

#ifndef EXTENDED_STARTUPINFO_PRESENT
#define EXTENDED_STARTUPINFO_PRESENT 0x00080000
#endif

#ifndef PTY_SET_LOG
#define PTY_SET_LOG
struct _iobuf;
typedef struct _iobuf FILE;
void pty_set_log_file(FILE* f);
#endif

class Pty {
public:
    Pty();
    ~Pty();

    bool spawn(const std::wstring& program = L"", const std::wstring& cmdline = L"");
    void close();
    void resize(int cols, int rows);
    bool write(const char* data, size_t len);

    bool isRunning() const { return m_running; }

    std::function<void(const char*, size_t)> onData;
    std::function<void()> onExit;

private:
    void readThread();

    HANDLE m_inputWrite = INVALID_HANDLE_VALUE;
    HANDLE m_outputRead = INVALID_HANDLE_VALUE;
    HANDLE m_process = INVALID_HANDLE_VALUE;
    HANDLE m_thread = INVALID_HANDLE_VALUE;
    HPCON m_conpty = nullptr;

    std::atomic<bool> m_running{false};
    std::thread m_readThread;

    int m_cols = 80;
    int m_rows = 25;
};
