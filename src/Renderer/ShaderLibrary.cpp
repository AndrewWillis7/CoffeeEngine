#include "ShaderLibrary.h"
#include "BuiltInShaders.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::unordered_map<std::string, ShaderLibrary::Entry>& ShaderLibrary::Table() {
    static std::unordered_map<std::string, Entry> table;
    return table;
}

void ShaderLibrary::Register(const std::string& name, const std::string& fragmentSr) {
    RegisterCustom(name, BuiltInShaders::QuadVertexSrc, fragmentSr);
}

void ShaderLibrary::RegisterCustom(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) {
    Table()[name] = Entry{vertexSrc, fragmentSrc};
}

bool ShaderLibrary::RegisterFromFile(const std::string& name, const std::string& fragmentPath) {
    std::ifstream file(fragmentPath);
    if (!file) {
        std::cerr << "Engine Warning: ShaderLibrary couldn't opem '" << fragmentPath << "' for shader '" << name << "'\n";
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    Register(name, buffer.str());
    return true;
}

const ShaderLibrary::Entry* ShaderLibrary::Find(const std::string& name) {
    auto it = Table().find(name);
    return it != Table().end() ? &it->second : nullptr;
}