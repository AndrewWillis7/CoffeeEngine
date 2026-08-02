#include "Renderer2D.h"
#include "GLLoader.h"
#include "Shader.h"
#include "BuiltInShaders.h"
#include <GL/gl.h>
#include <iostream>

namespace {
// Unit quad, centered on the origin -- the vertex shader scales this by
// u_Size and rotates/translates it into world space.
constexpr float kQuadVertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,
    -0.5f,  0.5f,
};
} // End Of Namespace

Renderer2D::Renderer2D() = default;

Renderer2D::~Renderer2D() {
    Shutdown();
}

void Renderer2D::Init() {
    if (!GL::Load()) {
        std::cerr << "Engine Fatal: Renderer2D failed to load required GL functions. "
                     "Shader-based drawing will not work.\n";
        return;
    }

    GL::GenBuffers(1, &m_VBO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    GL::BindBuffer(GL_ARRAY_BUFFER, 0);

    m_DefaultShader = std::make_unique<Shader>(BuiltInShaders::QuadVertexSrc, BuiltInShaders::FlatFragmentSrc);
    if (!m_DefaultShader->IsValid()) {
        std::cerr << "Engine Fatal: Renderer2D's built-in flat shader failed to build.\n";
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_Initialized = true;
    std::cout << "Renderer2D initialized (shader pipeline ready)\n";
}

void Renderer2D::Shutdown() {
    m_DefaultShader.reset();
    if (m_VBO) {
        GL::DeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    m_Initialized = false;
}

void Renderer2D::SetViewportSize(int width, int height) {
    m_Width = width > 0 ? static_cast<float>(width) : 1.0f;
    m_Height = height > 0 ? static_cast<float>(height) : 1.0f;
    glViewport(0, 0, width, height);
}

void Renderer2D::BeginFrame(float deltaTime) {
    m_Time += deltaTime;
}

void Renderer2D::ApplyCommonUniforms(Shader& shader, const Transform2D& transform, const Vector2& size, const Color& color) const {
    shader.SetVec2("u_Resolution", m_Width, m_Height);
    shader.SetFloat("u_Time", m_Time);
    shader.SetVec2("u_Position", transform.position.x, transform.position.y);
    shader.SetVec2("u_Size", size.x, size.y);
    shader.SetFloat("u_Rotation", transform.rotation);
    shader.SetVec4("u_Color", color.r, color.g, color.b, color.a);
}

void Renderer2D::DrawQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader) {
    if (!m_Initialized) return;

    Shader* active = (shader && shader->IsValid()) ? shader : m_DefaultShader.get();
    if (!active || !active->IsValid()) return;

    Vector2 drawSize = size * active->overdrawScale;

    active->Bind();
    ApplyCommonUniforms(*active, transform, drawSize, color);

    GL::BindBuffer(GL_ARRAY_BUFFER, m_VBO);

    GLint posAttrib = GL::GetAttribLocation(active->GetProgram(), "a_LocalPos");
    if (posAttrib >= 0) {
        GL::EnableVertexAttribArray(static_cast<GLuint>(posAttrib));
        GL::VertexAttribPointer(static_cast<GLuint>(posAttrib), 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if (posAttrib >= 0) {
        GL::DisableVertexAttribArray(static_cast<GLuint>(posAttrib));
    }

    GL::BindBuffer(GL_ARRAY_BUFFER, 0);
    Shader::Unbind();
}