#pragma once
#include <string>
#include <unordered_map>

// A registry of GLSL source keyed by name. Compiles nothing and owns no GL
// objects -- ActorRegistry still owns the shaders. All engine shader source
// lives on disk under scripts/shaders/; this reads it in and hands it out.
class ShaderLibrary {
public:
    struct Entry {
        std::string vertexSrc;
        std::string fragmentSrc;
    };

    // Pairs fragmentSrc with the shared vertex stage.
    static void Register(const std::string& name, const std::string& fragmentSrc);

    // Fully custom vertex stage too.
    static void RegisterCustom(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);

    // Reads a .frag off disk and registers it, so GLSL can be iterated on without
    // recompiling C++. False (and a warning) if the file can't be opened.
    static bool RegisterFromFile(const std::string& name, const std::string& fragmentPath);

    // Every shader in the engine pairs its fragment stage against this one,
    // read from scripts/shaders/quad.vert once and cached for the program's
    // life. A missing file warns once and yields "", so shaders built from it
    // fail IsValid() like any other bad GLSL rather than crashing.
    static const std::string& SharedVertexSrc();

    // One canonical file->string helper, instead of three copies of the same
    // ifstream dance. False (leaving `out` untouched) if it can't be opened.
    static bool ReadFile(const std::string& path, std::string& out);

    static const Entry* Find(const std::string& name);

private:
    static std::unordered_map<std::string, Entry>& Table();
};