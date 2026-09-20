#include "DebugLog.h"
#include <iostream>
#include <streambuf>

namespace {

// Deep enough to hold a whole startup plus a burst of Lua errors, shallow
// enough that the panel never has to page through it.
constexpr size_t kMaxEntries = 400;

std::deque<DebugLog::Entry> g_Entries;
unsigned int g_Warnings = 0;
unsigned int g_Errors = 0;

// Forwards every character to the stream it replaced while splitting the same
// text into lines for the ring buffer. Buffer-less by design: overflow() sees
// each character, which is exactly the granularity line splitting needs.
class TeeBuf : public std::streambuf {
public:
    TeeBuf(std::streambuf* target, DebugLog::Severity severity)
        : m_Target(target), m_Severity(severity) {}

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);

        const char c = traits_type::to_char_type(ch);
        if (c == '\n') {
            DebugLog::Push(m_Severity, m_Line);
            m_Line.clear();
        } else if (c != '\r') {
            m_Line.push_back(c);
        }
        return m_Target ? m_Target->sputc(c) : ch;
    }

    int sync() override { return m_Target ? m_Target->pubsync() : 0; }

private:
    std::streambuf* m_Target;
    DebugLog::Severity m_Severity;
    std::string m_Line;
};

// Deliberately leaked. Restoring the original buffers in Shutdown() is what
// makes the streams safe again; freeing the tees on top of that would only open
// a window where a late write lands in freed memory.
TeeBuf* g_OutTee = nullptr;
TeeBuf* g_ErrTee = nullptr;
std::streambuf* g_OutOriginal = nullptr;
std::streambuf* g_ErrOriginal = nullptr;

bool Contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

void DebugLog::Install() {
    if (g_OutTee) return;

    g_OutOriginal = std::cout.rdbuf();
    g_ErrOriginal = std::cerr.rdbuf();

    g_OutTee = new TeeBuf(g_OutOriginal, Severity::Info);
    g_ErrTee = new TeeBuf(g_ErrOriginal, Severity::Warning);

    std::cout.rdbuf(g_OutTee);
    std::cerr.rdbuf(g_ErrTee);
}

void DebugLog::Shutdown() {
    if (g_OutOriginal) std::cout.rdbuf(g_OutOriginal);
    if (g_ErrOriginal) std::cerr.rdbuf(g_ErrOriginal);
    g_OutOriginal = nullptr;
    g_ErrOriginal = nullptr;
}

void DebugLog::Push(Severity severity, const std::string& text) {
    if (text.empty()) return;

    // The engine prefixes its own messages, so a line arriving on cerr can be
    // graded more precisely than "something wrote to the error stream".
    if (severity == Severity::Warning && (Contains(text, "Fatal") || Contains(text, "error"))) {
        severity = Severity::Error;
    }

    if (!g_Entries.empty() && g_Entries.back().text == text && g_Entries.back().severity == severity) {
        ++g_Entries.back().repeats;
        return;
    }

    if (severity == Severity::Warning) ++g_Warnings;
    else if (severity == Severity::Error) ++g_Errors;

    g_Entries.push_back({severity, text, 1});
    while (g_Entries.size() > kMaxEntries) g_Entries.pop_front();
}

const std::deque<DebugLog::Entry>& DebugLog::Entries() { return g_Entries; }

void DebugLog::Clear() {
    g_Entries.clear();
    g_Warnings = 0;
    g_Errors = 0;
}

unsigned int DebugLog::WarningCount() { return g_Warnings; }
unsigned int DebugLog::ErrorCount() { return g_Errors; }
