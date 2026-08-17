#include "ShaderLibrary.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace {
// Every named/default shader in the engine pairs its fragment stage
// against this one vertex stage -- see ShaderLibrary::SharedVertexSrc().
constexpr const char* kSharedVertexPath = "scripts/shaders/quad.vert";
} // End Of Namespace

std::unordered_map<std::string, ShaderLibrary::Entry>& ShaderLibrary::Table() {
    static std::unordered_map<std::string, Entry> table;
    return table;
}

bool ShaderLibrary::ReadFile(const std::string& path, std::string& out) {
    std::ifstream file(path);
    if (!file) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

const std::string& ShaderLibrary::SharedVertexSrc() {
    // Read once, lazily, the first time anything asks -- regardless of
    // whether that's ActorRegistry's constructor or Renderer2D::Init(),
    // whichever runs first (main.cpp constructs Renderer2D before
    // ActorRegistry). Cached for the rest of the program's life since
    // this vertex stage never changes at runtime.
    static const std::string src = [] {
        std::string text;
        if (!ReadFile(kSharedVertexPath, text)) {
            std::cerr << "Engine Fatal: ShaderLibrary couldn't open '" << kSharedVertexPath
                       << "' -- every shader in the engine pairs against this vertex stage; "
                          "rendering will not work.\n";
        }
        return text;
    }();
    return src;
}

void ShaderLibrary::Register(const std::string& name, const std::string& fragmentSrc) {
    RegisterCustom(name, SharedVertexSrc(), fragmentSrc);
}

void ShaderLibrary::RegisterCustom(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) {
    Table()[name] = Entry{vertexSrc, fragmentSrc};
}

bool ShaderLibrary::RegisterFromFile(const std::string& name, const std::string& fragmentPath) {
    std::string fragmentSrc;
    if (!ReadFile(fragmentPath, fragmentSrc)) {
        std::cerr << "Engine Warning: ShaderLibrary couldn't open '" << fragmentPath << "' for shader '" << name << "'\n";
        return false;
    }
    Register(name, fragmentSrc);
    return true;
}

const ShaderLibrary::Entry* ShaderLibrary::Find(const std::string& name) {
    auto it = Table().find(name);
    return it != Table().end() ? &it->second : nullptr;
}