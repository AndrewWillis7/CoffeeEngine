#pragma once
#include <string>
#include <unordered_map>

// A registry of GLSL source, keyed by name, doesnt compile or own GL Objects
// Actor registry still owns the shaders. This just helps with adding new shaders
//
// All engine shader source lives on disk under scripts/shaders/ (no
// hardcoded GLSL strings in the C++ anymore) -- this class is what
// reads it in and hands it out.
class ShaderLibrary {
public:
    struct Entry {
        std::string vertexSrc;
        std::string fragmentSrc;
    };

    // Common case: pairs fragmentSrc with the engine's shared vertex
    // stage -- see SharedVertexSrc().
    static void Register(const std::string& name, const std::string& fragmentSrc);

    // Rare case: fully custom vertex stage too
    static void RegisterCustom(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);

    // Reads a .frag file off disk and registers it under name, paired
    // with SharedVertexSrc(). Lets us iterate on GLSL without
    // recompiling C++. Returns false (and logs a warning) if the file
    // can't be opened.
    static bool RegisterFromFile(const std::string& name, const std::string& fragmentPath);

    // The engine's shared vertex stage -- every shader (named or not,
    // including Renderer2D's own default flat shader) pairs its
    // fragment stage against this same vertex stage, read from
    // scripts/shaders/quad.vert once and cached for the rest of the
    // program's life. A missing/unreadable file logs one warning and
    // yields "" -- every Shader built from it then just fails
    // IsValid(), same as any other bad-GLSL case, instead of crashing.
    static const std::string& SharedVertexSrc();

    // Small file->string helper used by RegisterFromFile and by
    // anything else (Renderer2D's default shader, ActorRegistry's
    // LoadNamedShaderFromFile) that needs to read a raw GLSL file off
    // disk -- one canonical place for that instead of three copies of
    // the same ifstream/stringstream dance. Returns false (leaving
    // `out` untouched) if the file can't be opened.
    static bool ReadFile(const std::string& path, std::string& out);

    static const Entry* Find(const std::string& name);

private:
    static std::unordered_map<std::string, Entry>& Table();
};