#include "Renderer2D.h"
#include "GLLoader.h"
#include "Shader.h"
#include "Texture.h"
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

void Renderer2D::SetActiveCamera(const Vector2& position, const Vector2& viewportSize) {
    m_HasCamera = true;
    m_CameraPos = position;
    // Guard against a zero/negative viewport (e.g. a Camera2D whose
    // viewportSize was never set, or set wrong from Lua) -- the vertex
    // shader divides by this, so a zero here would NaN out every
    // world-space draw for the rest of the frame instead of just looking
    // wrong for that one camera.
    m_CameraViewport = Vector2(
        viewportSize.x > 0.0f ? viewportSize.x : 1.0f,
        viewportSize.y > 0.0f ? viewportSize.y : 1.0f);
}

void Renderer2D::ClearActiveCamera() {
    m_HasCamera = false;
}

void Renderer2D::ApplyCommonUniforms(Shader& shader, const Transform2D& transform, const Vector2& size,
                                      const Color& color, bool world) const {
    // Identity mapping -- world pixels line up 1:1 with screen pixels,
    // origin at the window's center. This is both the "no camera set"
    // fallback for world-space draws AND exactly what every screen-space
    // (UI) draw always uses, so there's only one formula to keep correct.
    Vector2 cameraPos{m_Width * 0.5f, m_Height * 0.5f};
    Vector2 viewport{m_Width, m_Height};

    if (world && m_HasCamera) {
        cameraPos = m_CameraPos;
        viewport = m_CameraViewport;
    }

    shader.SetVec2("u_Resolution", m_Width, m_Height);
    shader.SetFloat("u_Time", m_Time);
    shader.SetVec2("u_Position", transform.position.x, transform.position.y);
    shader.SetVec2("u_Size", size.x, size.y);
    shader.SetFloat("u_Rotation", transform.rotation);
    shader.SetVec4("u_Color", color.r, color.g, color.b, color.a);
    shader.SetVec2("u_CameraPos", cameraPos.x, cameraPos.y);
    shader.SetVec2("u_ViewportSize", viewport.x, viewport.y);
}

void Renderer2D::SubmitQuad(Shader& active) {
    GL::BindBuffer(GL_ARRAY_BUFFER, m_VBO);

    GLint posAttrib = GL::GetAttribLocation(active.GetProgram(), "a_LocalPos");
    if (posAttrib >= 0) {
        GL::EnableVertexAttribArray(static_cast<GLuint>(posAttrib));
        GL::VertexAttribPointer(static_cast<GLuint>(posAttrib), 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if (posAttrib >= 0) {
        GL::DisableVertexAttribArray(static_cast<GLuint>(posAttrib));
    }

    GL::BindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer2D::DrawQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader) {
    if (!m_Initialized) return;

    Shader* active = (shader && shader->IsValid()) ? shader : m_DefaultShader.get();
    if (!active || !active->IsValid()) return;

    Vector2 drawSize = size * active->overdrawScale;

    active->Bind();
    ApplyCommonUniforms(*active, transform, drawSize, color, /*world=*/true);
    SubmitQuad(*active);
    Shader::Unbind();
}

void Renderer2D::DrawTexturedQuad(const Transform2D& transform, const Vector2& size, const Color& tint,
                                   Shader* shader, Texture* texture, Vector2 uvOffset, Vector2 uvScale) {
    if (!m_Initialized || !texture || !texture->IsValid()) return;
    if (!shader || !shader->IsValid()) return;

    Vector2 drawSize = size * shader->overdrawScale;

    texture->Bind();
    shader->Bind();
    ApplyCommonUniforms(*shader, transform, drawSize, tint, /*world=*/true);
    shader->SetInt("u_Texture", 0);
    shader->SetVec2("u_UVOffset", uvOffset.x, uvOffset.y);
    shader->SetVec2("u_UVScale", uvScale.x, uvScale.y);
    SubmitQuad(*shader);
    Shader::Unbind();
}

void Renderer2D::DrawScreenQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader) {
    if (!m_Initialized) return;

    Shader* active = (shader && shader->IsValid()) ? shader : m_DefaultShader.get();
    if (!active || !active->IsValid()) return;

    Vector2 drawSize = size * active->overdrawScale;

    active->Bind();
    ApplyCommonUniforms(*active, transform, drawSize, color, /*world=*/false);
    SubmitQuad(*active);
    Shader::Unbind();
}

void Renderer2D::DrawScreenTexturedQuad(const Transform2D& transform, const Vector2& size, const Color& tint,
                                         Shader* shader, Texture* texture, Vector2 uvOffset, Vector2 uvScale) {
    if (!m_Initialized || !texture || !texture->IsValid()) return;
    if (!shader || !shader->IsValid()) return;

    Vector2 drawSize = size * shader->overdrawScale;

    texture->Bind();
    shader->Bind();
    ApplyCommonUniforms(*shader, transform, drawSize, tint, /*world=*/false);
    shader->SetInt("u_Texture", 0);
    shader->SetVec2("u_UVOffset", uvOffset.x, uvOffset.y);
    shader->SetVec2("u_UVScale", uvScale.x, uvScale.y);
    SubmitQuad(*shader);
    Shader::Unbind();
}