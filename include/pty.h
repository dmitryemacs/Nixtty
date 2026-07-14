#pragma once

#include <cstdio>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

void pty_set_log_file(FILE* f);

class Pty {
public:
    Pty();
    ~Pty();

    bool spawn(const std::string& shell = "");
    void close();
    void resize(int cols, int rows);
    bool write(const char* data, size_t len);

    bool isRunning() const { return m_running; }

    std::function<void(const char*, size_t)> onData;
    std::function<void()> onExit;

private:
    void readThread();

    int m_masterFd = -1;
    pid_t m_childPid = -1;

    std::atomic<bool> m_running{false};
    std::thread m_readThread;

    int m_cols = 80;
    int m_rows = 25;
};
