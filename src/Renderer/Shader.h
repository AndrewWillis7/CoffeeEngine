#pragma once
#include <string>
#include <unordered_map>

// Thin wrapper around a compiled and linked GLSL program, owning the GL program
// object for its lifetime. Instances are created and owned by ActorRegistry.

class Shader {
public:
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Bind() const;
    static void Unbind();

    bool IsValid() const {return m_Program != 0;}
    unsigned int GetProgram() const {return m_Program;}

    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, float x, float y);
    void SetVec3(const std::string& name, float x, float y, float z);
    void SetVec4(const std::string& name, float x, float y, float z, float w);

    void SetInt(const std::string& name, int value);
    int GetAttribLocation(const std::string& name);

    // How much bigger than the body's logical size to draw the quad.
    float overdrawScale = 1.0f;

private:
    int GetUniformLocation(const std::string& name);

    unsigned int m_Program = 0;
    std::unordered_map<std::string, int> m_UniformLocationCache;
    std::unordered_map<std::string, int> m_AttribLocationCache;
};