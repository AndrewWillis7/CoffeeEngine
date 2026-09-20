#pragma once
#include <deque>
#include <string>

// Engine output, readable from inside the game rather than only in the terminal
// the exe was launched from -- which, started from Explorer on Windows, is not
// there at all.
//
// Install() tees std::cout and std::cerr into a bounded ring buffer and keeps
// forwarding both to the real streams, so nothing that used to print stops
// printing. ScriptEngine additionally routes Lua print() through std::cout for
// the same reason: Lua writes to stdout directly and would otherwise slip past
// the tee entirely.
class DebugLog {
public:
    enum class Severity { Info, Warning, Error };

    struct Entry {
        Severity severity = Severity::Info;
        std::string text;
        unsigned int repeats = 1; // identical consecutive lines collapse into one
    };

    // Safe to call more than once; the second call does nothing.
    static void Install();

    // Puts the original stream buffers back. Call before main returns, or
    // anything logging during static destruction writes through a dead tee.
    static void Shutdown();

    static void Push(Severity severity, const std::string& text);

    static const std::deque<Entry>& Entries();
    static void Clear();
    static unsigned int WarningCount();
    static unsigned int ErrorCount();
};
