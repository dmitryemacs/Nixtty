#pragma once

#include "terminal.h"
#include <vector>
#include <cstdint>

class AnsiParser {
public:
    AnsiParser(Terminal& terminal);

    void parse(const char* data, size_t len);
    void parse(const wchar_t* data, size_t len);

private:
    void processChar(wchar_t ch);
    void processEscapeSequence();
    void processCsiSequence();
    void processOscSequence();

    void executeCsi();
    void executeSgr();

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

    // UTF-8 decode state
    int m_utf8Expected = 0;
    uint32_t m_utf8Codepoint = 0;
};
