#include "Renderer2D.h"
#include "GLLoader.h"
#include "Shader.h"
#include "Texture.h"
#include "ShaderLibrary.h"
#include <GL/gl.h>
#include <algorithm>
#include <iostream>

namespace {
// Unit quad on the origin; the vertex shader scales, rotates and translates it.
constexpr float kQuadVertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,
    -0.5f,  0.5f,
};

// Largest centred rect of aspect targetW:targetH fitting inside the container,
// via a single uniform scale -- the never-stretch guarantee.
struct FitRect { float x, y, w, h; };

FitRect FitAspect(float containerW, float containerH, float targetW, float targetH) {
    if (targetW <= 0.0f) targetW = 1.0f;
    if (targetH <= 0.0f) targetH = 1.0f;

    float scale = std::min(containerW / targetW, containerH / targetH);
    float w = targetW * scale;
    float h = targetH * scale;
    return FitRect{ (containerW - w) * 0.5f, (containerH - h) * 0.5f, w, h };
}
} // namespace

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
    // Bound once and left bound for the program's life: this is the only array
    // buffer in the engine, so SubmitQuad never has to re-bind it.
    GL::BindBuffer(GL_ARRAY_BUFFER, m_VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

    // Used whenever a draw passes a null or invalid shader.
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
    if (m_ActiveAttrib >= 0) {
        GL::DisableVertexAttribArray(static_cast<GLuint>(m_ActiveAttrib));
        m_ActiveAttrib = -1;
    }
    if (m_VBO) {
        GL::BindBuffer(GL_ARRAY_BUFFER, 0);
        GL::DeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    m_Initialized = false;
}

void Renderer2D::SetViewportSize(int width, int height) {
    m_Width = width > 0 ? static_cast<float>(width) : 1.0f;
    m_Height = height > 0 ? static_cast<float>(height) : 1.0f;

    // Unconditional, even if already tracked as FullWindow: the window just
    // changed size, so the last actual glViewport call is stale either way.
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

void Renderer2D::SetPixelScale(float scale) {
    m_PixelScale = scale > 0.0f ? scale : 1.0f;
}

void Renderer2D::SetActiveCamera(const Vector2& position, const Vector2& viewportSize,
                                  const Vector2& targetAspect, Shader* borderShader,
                                  Texture* borderTexture) {
    m_HasCamera = true;
    m_CameraPos = position;
    // The vertex shader divides by this, so a zero from an unset Camera2D would
    // NaN out every world draw for the rest of the frame, not just this camera.
    m_CameraViewport = Vector2(
        viewportSize.x > 0.0f ? viewportSize.x : 1.0f,
        viewportSize.y > 0.0f ? viewportSize.y : 1.0f);

    // Nested aspect-fit. With targetAspect unset both steps use the same aspect,
    // so the second fit fills the first rect exactly -- one formula for both the
    // "fit to window" and "fit inside a specific shape" cases.
    bool hasTargetAspect = targetAspect.x > 0.0f && targetAspect.y > 0.0f;
    Vector2 aspectBasis = hasTargetAspect ? targetAspect : m_CameraViewport;

    FitRect outer = FitAspect(m_Width, m_Height, aspectBasis.x, aspectBasis.y);
    FitRect inner = FitAspect(outer.w, outer.h, m_CameraViewport.x * m_PixelScale, m_CameraViewport.y * m_PixelScale);

    m_ContentX = outer.x + inner.x;
    m_ContentY = outer.y + inner.y;
    m_ContentW = inner.w;
    m_ContentH = inner.h;

    // Paint the border into the margins the fit leaves behind. Must happen on the
    // FULL window viewport, before shrinking to the content rect below.
    if (borderShader && borderShader->IsValid()) {
        // Real screen pixels per native pixel, so a procedural border can quantize
        // onto the same grid the rest of the pixel art uses at any window size.
        // FitAspect only ever applies a uniform scale, so either axis works.
        // m_PixelScale folds in too, or the border's stars and clouds would keep
        // their old grain after a script chunks up the world.
        float pixelScale = (m_CameraViewport.x > 0.0f ? (inner.w / m_CameraViewport.x) : 1.0f) * m_PixelScale;
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
    // Identity mapping: world pixels 1:1 with screen pixels, origin at the window
    // centre. Both the no-camera fallback and what every UI draw always uses.
    Vector2 cameraPos{m_Width * 0.5f, m_Height * 0.5f};
    Vector2 viewport{m_Width, m_Height};

    if (world && m_HasCamera) {
        cameraPos = m_CameraPos;
        viewport = m_CameraViewport;
    }

    // Shrinking the effective viewport packs the same content rect with fewer
    // world units, so each draws bigger. World-space only; UI stays crisp.
    if (world && m_PixelScale != 1.0f) {
        viewport = viewport / m_PixelScale;
    }

    // Kept separate from u_ViewportSize (which drives the position math) for
    // fragment shaders wanting real screen pixels regardless of camera or pixel
    // scale -- border.frag's star and cloud grid, for instance.
    shader.SetVec2("u_Resolution", m_Width, m_Height);

    shader.SetFloat("u_Time", m_Time);
    shader.SetVec2("u_Position", transform.position.x, transform.position.y);
    shader.SetVec2("u_Size", size.x, size.y);
    shader.SetFloat("u_Rotation", transform.rotation);
    shader.SetVec4("u_Color", color.r, color.g, color.b, color.a);
    shader.SetVec2("u_CameraPos", cameraPos.x, cameraPos.y);
    shader.SetVec2("u_ViewportSize", viewport.x, viewport.y);

    // World draws snap their centre to a whole native pixel, so the art stays
    // crisp while Camera2D::Follow's lerp sits on a fractional position; without
    // it sprites shimmer as the camera eases. Screen draws deliberately don't --
    // UI shouldn't be forced onto the game's grid, and the border gets its own
    // on-grid look from u_PixelScale instead.
    shader.SetFloat("u_PixelSnap", world ? 1.0f : 0.0f);
}

void Renderer2D::SubmitQuad(Shader& active) {
    // Through Shader's own cache, not GL::GetAttribLocation -- this runs once per
    // quad, and some drivers implicitly sync on a by-name query.
    GLint posAttrib = active.GetAttribLocation("a_LocalPos");
    if (posAttrib < 0) return;

    // The VBO stays bound from Init(), and every shader here uses the same vertex
    // layout, so the attrib only needs re-pointing when its index changes --
    // which in practice means once, on the first draw.
    if (posAttrib != m_ActiveAttrib) {
        if (m_ActiveAttrib >= 0) GL::DisableVertexAttribArray(static_cast<GLuint>(m_ActiveAttrib));
        GL::EnableVertexAttribArray(static_cast<GLuint>(posAttrib));
        GL::VertexAttribPointer(static_cast<GLuint>(posAttrib), 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        m_ActiveAttrib = posAttrib;
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Renderer2D::DrawQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader) {
    if (!m_Initialized) return;

    EnsureViewport(m_HasCamera ? ViewportMode::Content : ViewportMode::FullWindow);

    Shader* active = (shader && shader->IsValid()) ? shader : m_DefaultShader.get();
    if (!active || !active->IsValid()) return;

    // transform.scale multiplies `size` and the shader's overdrawScale, drawing
    // bigger without touching the object's logical/collision size.
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

void Renderer2D::DrawScreenQuadBatch(const ScreenQuad* quads, size_t count, Shader* shader, Texture* texture) {
    if (!m_Initialized || !quads || count == 0) return;
    if (texture && !texture->IsValid()) return;

    Shader* active = (shader && shader->IsValid()) ? shader : m_DefaultShader.get();
    if (!active || !active->IsValid()) return;

    EnsureViewport(ViewportMode::FullWindow);

    if (texture) texture->Bind();
    active->Bind();

    // Everything ApplyCommonUniforms would otherwise resend per quad. Identical
    // for the whole batch -- screen space, unrotated, unsnapped -- so it goes up
    // once and the loop below only touches position, size, color and UVs.
    active->SetVec2("u_Resolution", m_Width, m_Height);
    active->SetFloat("u_Time", m_Time);
    active->SetFloat("u_Rotation", 0.0f);
    active->SetVec2("u_CameraPos", m_Width * 0.5f, m_Height * 0.5f);
    active->SetVec2("u_ViewportSize", m_Width, m_Height);
    active->SetFloat("u_PixelSnap", 0.0f);
    if (texture) active->SetInt("u_Texture", 0);

    const float overdraw = active->overdrawScale;
    for (size_t i = 0; i < count; ++i) {
        const ScreenQuad& q = quads[i];
        active->SetVec2("u_Position", q.center.x, q.center.y);
        active->SetVec2("u_Size", q.size.x * overdraw, q.size.y * overdraw);
        active->SetVec4("u_Color", q.color.r, q.color.g, q.color.b, q.color.a);
        if (texture) {
            active->SetVec2("u_UVOffset", q.uvOffset.x, q.uvOffset.y);
            active->SetVec2("u_UVScale", q.uvScale.x, q.uvScale.y);
        }
        SubmitQuad(*active);
    }

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