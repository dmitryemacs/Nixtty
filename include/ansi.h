#pragma once

#include "terminal.h"
#include <vector>
#include <cstdint>
#include <functional>

class AnsiParser {
public:
    AnsiParser(Terminal& terminal);

    void parse(const char* data, size_t len);
    void parse(const wchar_t* data, size_t len);

    std::function<void(const char*, size_t)> onWrite;

private:
    void processChar(wchar_t ch);
    void executeCsi();
    void executeSgr();
    void executeOsc();
    void writeResponse(const char* data, size_t len);

    Terminal& m_terminal;

    enum State {
        STATE_GROUND,
        STATE_ESC,
        STATE_CSI,
        STATE_OSC,
        STATE_ESC_IGNORE,
    };
    State m_state = STATE_GROUND;

    std::vector<wchar_t> m_sequence;
    std::vector<int> m_params;
    int m_currentParam = 0;
    bool m_hasParam = false;
    bool m_csiPrivate = false;

    int m_utf8Expected = 0;
    uint32_t m_utf8Codepoint = 0;
};
