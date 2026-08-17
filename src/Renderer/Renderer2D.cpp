#include "Renderer2D.h"
#include "GLLoader.h"
#include "Shader.h"
#include "Texture.h"
#include "ShaderLibrary.h"
#include <GL/gl.h>
#include <algorithm>
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

// Largest rect of aspect targetW:targetH that fits inside a
// containerW x containerH box, centered, via a single uniform scale
// factor -- the "never stretch" guarantee: both axes always scale by
// exactly the same amount, the smaller of the two possible fits.
struct FitRect { float x, y, w, h; };

FitRect FitAspect(float containerW, float containerH, float targetW, float targetH) {
    if (targetW <= 0.0f) targetW = 1.0f;
    if (targetH <= 0.0f) targetH = 1.0f;

    float scale = std::min(containerW / targetW, containerH / targetH);
    float w = targetW * scale;
    float h = targetH * scale;
    return FitRect{ (containerW - w) * 0.5f, (containerH - h) * 0.5f, w, h };
}
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

    // The default shader used whenever a Draw call passes a null/invalid
    // Shader* -- source lives at scripts/shaders/flat.frag, paired with
    // the shared vertex stage every other shader in the engine uses too
    // (see ShaderLibrary::SharedVertexSrc()).
    std::string flatFragmentSrc;
    if (!ShaderLibrary::ReadFile("scripts/shaders/flat.frag", flatFragmentSrc)) {
        std::cerr << "Engine Fatal: Renderer2D couldn't open 'scripts/shaders/flat.frag'.\n";
        return;
    }
    m_DefaultShader = std::make_unique<Shader>(ShaderLibrary::SharedVertexSrc(), flatFragmentSrc);
    if (!m_DefaultShader->IsValid()) {
        std::cerr << "Engine Fatal: Renderer2D's default flat shader failed to build.\n";
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

    // Force a fresh glViewport(0,0,w,h) unconditionally, even if we happen
    // to already be tracked as FullWindow -- the window itself just
    // changed size, so the last actual glViewport call (whatever mode it
    // was in) is stale regardless of what our cached mode says.
    glViewport(0, 0, width, height);
    m_ViewportMode = ViewportMode::FullWindow;
}

void Renderer2D::BeginFrame(float deltaTime) {
    m_Time += deltaTime;
}

void Renderer2D::EnsureViewport(ViewportMode mode) {
    if (m_ViewportMode == mode) return;
    m_ViewportMode = mode;

    if (mode == ViewportMode::FullWindow) {
        glViewport(0, 0, static_cast<GLint>(m_Width), static_cast<GLint>(m_Height));
    } else {
        glViewport(static_cast<GLint>(m_ContentX), static_cast<GLint>(m_ContentY),
                   static_cast<GLsizei>(m_ContentW), static_cast<GLsizei>(m_ContentH));
    }
}

void Renderer2D::SetActiveCamera(const Vector2& position, const Vector2& viewportSize,
                                  const Vector2& targetAspect, Shader* borderShader,
                                  Texture* borderTexture) {
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

    // Nested aspect-fit: first fit targetAspect (or, if unset, the
    // camera's own viewportSize) into the real window; then fit
    // viewportSize into THAT rect. When targetAspect is unset the two
    // steps use the same aspect, so the second fit just fills the first
    // rect exactly -- one clean formula covers both the "just fit to
    // window" and "fit inside a specific on-screen shape" cases.
    bool hasTargetAspect = targetAspect.x > 0.0f && targetAspect.y > 0.0f;
    Vector2 aspectBasis = hasTargetAspect ? targetAspect : m_CameraViewport;

    FitRect outer = FitAspect(m_Width, m_Height, aspectBasis.x, aspectBasis.y);
    FitRect inner = FitAspect(outer.w, outer.h, m_CameraViewport.x, m_CameraViewport.y);

    m_ContentX = outer.x + inner.x;
    m_ContentY = outer.y + inner.y;
    m_ContentW = inner.w;
    m_ContentH = inner.h;

    // Paint the border into whatever margin space the fit above leaves
    // behind -- must happen on the FULL window viewport, before we shrink
    // down to the content rect below, or it'd just paint over itself.
    if (borderShader && borderShader->IsValid()) {
        // How many real screen pixels currently correspond to one
        // native/virtual pixel -- lets a procedural border shader (see
        // border.frag) quantize itself onto the SAME pixel grid the rest
        // of the game's pixel art renders at, regardless of the real
        // window's size. inner.w/m_CameraViewport.x and
        // inner.h/m_CameraViewport.y are equal (FitAspect only ever
        // applies a single uniform scale, never a non-uniform stretch),
        // so either axis works here; x is used for a single scalar.
        float pixelScale = m_CameraViewport.x > 0.0f ? (inner.w / m_CameraViewport.x) : 1.0f;
        borderShader->SetFloat("u_PixelScale", pixelScale);

        if (borderTexture && borderTexture->IsValid()) {
            DrawScreenTexturedQuad({{m_Width * 0.5f, m_Height * 0.5f}, 0.0f}, {m_Width, m_Height},
                                    Color::White(), borderShader, borderTexture);
        } else {
            DrawScreenQuad({{m_Width * 0.5f, m_Height * 0.5f}, 0.0f}, {m_Width, m_Height}, Color::White(), borderShader);
        }
    }

    EnsureViewport(ViewportMode::Content);
}

void Renderer2D::ClearActiveCamera() {
    m_HasCamera = false;
    EnsureViewport(ViewportMode::FullWindow);
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

    // On-grid pixel snapping: world-space game-object draws (DrawQuad/
    // DrawTexturedQuad) snap their center to the nearest whole native
    // pixel so the pixel art stays crisp as the camera smoothly follows
    // the player (Camera2D::Follow's exponential lerp lands on a
    // fractional position most frames) -- without this, sprites
    // shimmer/blur by fractions of a pixel as the camera eases toward
    // its target. Screen-space draws (DrawScreenQuad/
    // DrawScreenTexturedQuad -- the debug UI panel AND the full-window
    // border background) deliberately do NOT snap here: UI shouldn't be
    // forced onto the game's pixel grid, and the border achieves its own
    // on-grid look through a different mechanism entirely (see
    // border.frag's u_PixelScale), since it's one static full-window
    // quad, not a moving sprite.
    shader.SetFloat("u_PixelSnap", world ? 1.0f : 0.0f);
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

    EnsureViewport(m_HasCamera ? ViewportMode::Content : ViewportMode::FullWindow);

    Shader* active = (shader && shader->IsValid()) ? shader : m_DefaultShader.get();
    if (!active || !active->IsValid()) return;

    // transform.scale is a per-object multiplier on top of `size` (and the
    // shader's own overdrawScale) -- e.g. Vector2(2,2) draws twice as big
    // without touching the object's logical/collision size. Defaults to
    // Vector2::One() (see Transform2D.h), so this is a no-op for every
    // existing caller that's never touched .scale.
    Vector2 drawSize = size * active->overdrawScale * transform.scale;

    active->Bind();
    ApplyCommonUniforms(*active, transform, drawSize, color, /*world=*/true);
    SubmitQuad(*active);
    Shader::Unbind();
}

void Renderer2D::DrawTexturedQuad(const Transform2D& transform, const Vector2& size, const Color& tint,
                                   Shader* shader, Texture* texture, Vector2 uvOffset, Vector2 uvScale) {
    if (!m_Initialized || !texture || !texture->IsValid()) return;
    if (!shader || !shader->IsValid()) return;

    EnsureViewport(m_HasCamera ? ViewportMode::Content : ViewportMode::FullWindow);

    Vector2 drawSize = size * shader->overdrawScale * transform.scale;

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

    EnsureViewport(ViewportMode::FullWindow);

    Shader* active = (shader && shader->IsValid()) ? shader : m_DefaultShader.get();
    if (!active || !active->IsValid()) return;

    Vector2 drawSize = size * active->overdrawScale * transform.scale;

    active->Bind();
    ApplyCommonUniforms(*active, transform, drawSize, color, /*world=*/false);
    SubmitQuad(*active);
    Shader::Unbind();
}

void Renderer2D::DrawScreenTexturedQuad(const Transform2D& transform, const Vector2& size, const Color& tint,
                                         Shader* shader, Texture* texture, Vector2 uvOffset, Vector2 uvScale) {
    if (!m_Initialized || !texture || !texture->IsValid()) return;
    if (!shader || !shader->IsValid()) return;

    EnsureViewport(ViewportMode::FullWindow);

    Vector2 drawSize = size * shader->overdrawScale * transform.scale;

    texture->Bind();
    shader->Bind();
    ApplyCommonUniforms(*shader, transform, drawSize, tint, /*world=*/false);
    shader->SetInt("u_Texture", 0);
    shader->SetVec2("u_UVOffset", uvOffset.x, uvOffset.y);
    shader->SetVec2("u_UVScale", uvScale.x, uvScale.y);
    SubmitQuad(*shader);
    Shader::Unbind();
}