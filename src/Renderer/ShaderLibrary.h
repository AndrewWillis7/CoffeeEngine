#pragma once
#include <string>
#include <unordered_map>

// A registry of GLSL source, keyed by name, doesnt compile or own GL Objects
// Actor registry still owns the shaders. This just helps with adding new shaders
class ShaderLibrary {
public:
    struct Entry {
        std::string vertexSrc;
        std::string fragmentSrc;
    };

    // Common case: pairs fragmentSrc with the engines shared quad vertex
    static void Register(const std::string& name, const std::string& fragmentSrc);

    // Rare case: fully custom vertex stage too
    static void RegisterCustom(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);

    // Reads a .frag file off disk and registers it under name.
    // Lets us iterate on GLSL without recompiling C++
    static bool RegisterFromFile(const std::string& name, const std::string& fragmentPath);

    static const Entry* Find(const std::string& name);

private:
    static std::unordered_map<std::string, Entry>& Table();
};