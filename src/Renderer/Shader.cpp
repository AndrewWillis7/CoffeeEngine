#include "Shader.h"
#include "GLLoader.h"
#include <iostream>
#include <vector>

namespace {

GLuint s_CurrentProgram = 0;

// Compiles one stage; returns its GL handle, or 0 on failure.
GLuint CompileStage(GLenum stage, const std::string& source, const char* stageName) {
    GLuint handle = GL::CreateShader(stage);
    const char* src = source.c_str();
    GL::ShaderSource(handle, 1, &src, nullptr);
    GL::CompileShader(handle);

    GLint success = GL_FALSE;
    GL::GetShaderiv(handle, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        GL::GetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength > 0 ? logLength : 1);
        GL::GetShaderInfoLog(handle, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << "Engine Warning: " << stageName << " shader failed to compile:\n"
                    << log.data() << "\n";
        GL::DeleteShader(handle);
        return 0;
    }
    return handle;
}

} // namespace

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    GLuint vertex = CompileStage(GL_VERTEX_SHADER, vertexSrc, "Vertex");
    GLuint fragment = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc, "Fragment");

    if (!vertex || !fragment) {
        if (vertex) GL::DeleteShader(vertex);
        if (fragment) GL::DeleteShader(fragment);
        return; // m_Program stays 0 -- IsValid() reports the failure
    }

    GLuint program = GL::CreateProgram();
    GL::AttachShader(program, vertex);
    GL::AttachShader(program, fragment);
    GL::LinkProgram(program);

    GLint linked = GL_FALSE;
    GL::GetProgramiv(program, GL_LINK_STATUS, &linked);

    // Refcounted by the program once attached, so this is safe either way.
    GL::DeleteShader(vertex);
    GL::DeleteShader(fragment);

    if (!linked) {
        GLint logLength = 0;
        GL::GetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength > 0 ? logLength : 1);
        GL::GetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << "Engine Warning: Shader program failed to link:\n" << log.data() << "\n";
        GL::DeleteProgram(program);
        return;
    }

    m_Program = program;
}

Shader::~Shader() {
    if (m_Program) {
        GL::DeleteProgram(m_Program);
    }
}

void Shader::Bind() const {
    if (m_Program && s_CurrentProgram != m_Program) {
        GL::UseProgram(m_Program);
        s_CurrentProgram = m_Program;
    }
}

void Shader::Unbind() {
    GL::UseProgram(0);
    s_CurrentProgram = 0;
}

int Shader::GetUniformLocation(const std::string& name) {
    auto it = m_UniformLocationCache.find(name);
    if (it != m_UniformLocationCache.end()) return it->second;
    int location = GL::GetUniformLocation(m_Program, name.c_str());
    m_UniformLocationCache[name] = location;
    return location;
}

int Shader::GetAttribLocation(const std::string& name) {
    auto it = m_AttribLocationCache.find(name);
    if (it != m_AttribLocationCache.end()) return it->second;
    int location = GL::GetAttribLocation(m_Program, name.c_str());
    m_AttribLocationCache[name] = location;
    return location;
}

// glUniform* targets whatever program is currently bound, so each setter binds
// itself first -- Lua typically calls these right after creating a shader, long
// before it is ever used in a draw.

void Shader::SetFloat(const std::string& name, float value) {
    if (!m_Program) return;
    Bind();
    GL::Uniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, float x, float y) {
    if (!m_Program) return;
    Bind();
    GL::Uniform2f(GetUniformLocation(name), x, y);
}

void Shader::SetVec3(const std::string& name, float x, float y, float z) {
    if (!m_Program) return;
    Bind();
    GL::Uniform3f(GetUniformLocation(name), x, y, z);
}

void Shader::SetVec4(const std::string& name, float x, float y, float z, float w) {
    if (!m_Program) return;
    Bind();
    GL::Uniform4f(GetUniformLocation(name), x, y, z, w);
}

void Shader::SetInt(const std::string& name, int value) {
    if (!m_Program) return;
    Bind();
    GL::Uniform1i(GetUniformLocation(name), value);
}