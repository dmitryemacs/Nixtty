#include "pty.h"
#include <cstdio>
#include <chrono>
#include <thread>

Pty::Pty() : m_running(false), m_closing(false) {}
Pty::~Pty() { 
    close();
    // Отсоединяем поток если он все еще активен, чтобы избежать use-after-free
    if (m_readThread.joinable()) {
        m_readThread.detach();
    }
}

static FILE* pty_log_file = nullptr;

void pty_set_log_file(FILE* f) { pty_log_file = f; }

static void ptylog(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
    if (pty_log_file) { fprintf(pty_log_file, "[PTY] %s", buf); fflush(pty_log_file); }
}

bool Pty::spawn(const std::wstring& program, const std::wstring& cmdline) {
    HANDLE hConInRead = INVALID_HANDLE_VALUE;
    HANDLE hConInWrite = INVALID_HANDLE_VALUE;
    HANDLE hConOutRead = INVALID_HANDLE_VALUE;
    HANDLE hConOutWrite = INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;

    if (!CreatePipe(&hConInRead, &hConInWrite, &sa, 0)) {
        ptylog("ConIn pipe failed\n");
        return false;
    }
    if (!CreatePipe(&hConOutRead, &hConOutWrite, &sa, 0)) {
        CloseHandle(hConInRead); CloseHandle(hConInWrite);
        ptylog("ConOut pipe failed\n");
        return false;
    }

    COORD size = { (SHORT)m_cols, (SHORT)m_rows };
    HRESULT hr = CreatePseudoConsole(size, hConInRead, hConOutWrite, 0, &m_conpty);
    if (FAILED(hr)) {
        CloseHandle(hConInRead); CloseHandle(hConInWrite);
        CloseHandle(hConOutRead); CloseHandle(hConOutWrite);
        ptylog("CreatePseudoConsole failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    ptylog("ConPTY created, pipes OK\n");

    m_inputWrite = hConInWrite;
    m_outputRead = hConOutRead;

    STARTUPINFOEXW siEx = {};
    siEx.StartupInfo.cb = sizeof(siEx);
    siEx.StartupInfo.hStdInput = hConInRead;
    siEx.StartupInfo.hStdOutput = hConOutWrite;
    siEx.StartupInfo.hStdError = hConOutWrite;
    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    std::vector<BYTE> attrData(attrListSize);
    siEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)attrData.data();

    if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrListSize)) {
        ptylog("InitAttrList failed: %d\n", (int)GetLastError());
        close(); return false;
    }

    if (!UpdateProcThreadAttribute(siEx.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        m_conpty, sizeof(HPCON), nullptr, nullptr)) {
        ptylog("UpdateAttr failed: %d\n", (int)GetLastError());
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        close(); return false;
    }

    wchar_t shellPath[MAX_PATH] = {};

    if (!program.empty()) {
        wcscpy(shellPath, program.c_str());
    } else {
        wchar_t pwshPath[MAX_PATH] = {};
        GetEnvironmentVariableW(L"LOCALAPPDATA", pwshPath, MAX_PATH);
        wcscat(pwshPath, L"\\Microsoft\\WindowsApps\\pwsh.exe");
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pwshPath, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            wcscpy(shellPath, pwshPath);
        } else {
            GetWindowsDirectoryW(pwshPath, MAX_PATH);
            wcscat(pwshPath, L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
            hFind = FindFirstFileW(pwshPath, &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                FindClose(hFind);
                wcscpy(shellPath, pwshPath);
            } else {
                GetWindowsDirectoryW(shellPath, MAX_PATH);
                wcscat(shellPath, L"\\System32\\cmd.exe");
            }
        }
    }

    ptylog("Shell: %ls\n", shellPath);

    PROCESS_INFORMATION pi = {};
    BOOL result = CreateProcessW(nullptr, shellPath, nullptr, nullptr,
        FALSE, EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
        &siEx.StartupInfo, &pi);

    DeleteProcThreadAttributeList(siEx.lpAttributeList);

    CloseHandle(hConInRead);
    CloseHandle(hConOutWrite);

    if (!result) {
        ptylog("CreateProcessW failed: %d\n", (int)GetLastError());
        close(); return false;
    }

    m_process = pi.hProcess;
    m_thread = pi.hThread;
    m_running = true;

    m_readThread = std::thread(&Pty::readThread, this);

    ptylog("Shell spawned OK, PID=%d\n", (int)pi.dwProcessId);
    return true;
}

void Pty::readThread() {
    ptylog("ReadThread started, handle=%p, running=%d\n", m_outputRead, (int)m_running.load());
    char buffer[4096];
    while (m_running) {
        // Check if process has exited
        if (m_process != INVALID_HANDLE_VALUE) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(m_process, &exitCode) && exitCode != STILL_ACTIVE) {
                ptylog("Process exited with code %d\n", (int)exitCode);
                break;
            }
        }
        
        DWORD bytesRead = 0;
        BOOL success = ReadFile(m_outputRead, buffer, sizeof(buffer), &bytesRead, nullptr);

        if (!success || bytesRead == 0) {
            DWORD err = GetLastError();
            ptylog("ReadFile: success=%d bytes=%d err=%d\n", (int)success, (int)bytesRead, (int)err);
            if (err == ERROR_BROKEN_PIPE || !m_running) break;
            if (!success) { Sleep(10); continue; }
            break;
        }

        if (onData && bytesRead > 0) {
            onData(buffer, bytesRead);
        }
    }
    m_running = false;
    ptylog("ReadThread exit\n");
    // Notify main thread that pty process has exited
    if (onExit && !m_closing) {
        onExit();
    }
}

void Pty::resize(int cols, int rows) {
    if (!m_conpty) return;
    m_cols = cols;
    m_rows = rows;
    COORD size = { (SHORT)cols, (SHORT)rows };
    ResizePseudoConsole(m_conpty, size);
}

bool Pty::write(const char* data, size_t len) {
    if (m_inputWrite == INVALID_HANDLE_VALUE || !m_running) return false;
    DWORD bytesWritten = 0;
    return WriteFile(m_inputWrite, data, (DWORD)len, &bytesWritten, nullptr) != FALSE;
}

void Pty::close() {
    if (m_closing) return;
    m_closing = true;
    m_running = false;
    
    // Сначала терминируем процесс
    if (m_process != INVALID_HANDLE_VALUE) {
        TerminateProcess(m_process, 1);
        CloseHandle(m_process);
        m_process = INVALID_HANDLE_VALUE;
    }
    
    // Закрываем хендлы чтобы readThread завершился сам
    // Не ждем поток здесь, чтобы не блокировать GUI
    if (m_inputWrite != INVALID_HANDLE_VALUE) { CloseHandle(m_inputWrite); m_inputWrite = INVALID_HANDLE_VALUE; }
    if (m_outputRead != INVALID_HANDLE_VALUE) { CloseHandle(m_outputRead); m_outputRead = INVALID_HANDLE_VALUE; }
    if (m_conpty) { ClosePseudoConsole(m_conpty); m_conpty = nullptr; }
    if (m_thread != INVALID_HANDLE_VALUE) { CloseHandle(m_thread); m_thread = INVALID_HANDLE_VALUE; }
}
