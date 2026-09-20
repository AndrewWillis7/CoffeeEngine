#pragma once
#include "Core/Math/Transform2D.h"
#include "Core/Math/Color.h"
#include "Texture.h"
#include <memory>

class Shader;

// The shader-based, VBO-backed drawing pipeline. Knows nothing about the OS.
// Assumes a current GL context.
class Renderer2D {
public:
    Renderer2D();
    ~Renderer2D();

    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    // Call once after the GL context is current.
    void Init();
    void Shutdown();

    // Call at startup and on every window resize.
    void SetViewportSize(int width, int height);

    // Call once per frame before any draws.
    void BeginFrame(float deltaTime);

    // Sets the camera world-space draws map against for the rest of the frame.
    //   position      -- world point mapped to the centre of the content rect.
    //   viewportSize  -- how many world units span that rect, whatever its pixel
    //                    size; see Camera2D for why this is the resolution knob.
    //   targetAspect  -- optional (Vector2::Zero() for unset). Forces the content
    //                    rect into a specific on-screen shape.
    //   borderShader  -- drawn as one full-window quad BEFORE the viewport
    //                    narrows to the content rect, so it fills the margins.
    //                    Null leaves the margins showing whatever glClear left.
    //   borderTexture -- optional; lets a PixelSprite back the border.
    //
    // The content rect is a nested aspect-fit: fit targetAspect (or viewportSize)
    // into the window, then fit viewportSize into that. Either step degenerates
    // to an exact fill when the aspects already match, so one formula covers
    // every case without branching. Screen-space draws are never affected --
    // that split is the whole point.
    void SetActiveCamera(const Vector2& position, const Vector2& viewportSize,
                          const Vector2& targetAspect, Shader* borderShader,
                          Texture* borderTexture = nullptr);

    // Back to the identity mapping: world pixels == screen pixels, full window,
    // origin top-left. Also restores the full-window GL viewport.
    void ClearActiveCamera();

    // World-space: mapped through the active camera if there is one, otherwise
    // the identity mapping, so a script that never makes a camera draws as before.
    void DrawQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader);

    void DrawTexturedQuad(const Transform2D& transform, const Vector2& size, const Color& tint,
                        Shader* shader, Texture* texture,
                        Vector2 uvOffset = {0.0f, 0.0f}, Vector2 uvScale = {1.0f, 1.0f});

    // Screen-space: ALWAYS 1:1 against the real window, ignoring the camera.
    // The debug UI uses these so it stays glued to the screen rather than
    // panning and zooming with the game.
    void DrawScreenQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader);

    void DrawScreenTexturedQuad(const Transform2D& transform, const Vector2& size, const Color& tint,
                        Shader* shader, Texture* texture,
                        Vector2 uvOffset = {0.0f, 0.0f}, Vector2 uvScale = {1.0f, 1.0f});

    // One unrotated screen-space quad in a batch. uv* is ignored when the batch
    // has no texture.
    struct ScreenQuad {
        Vector2 center;
        Vector2 size;
        Color color;
        Vector2 uvOffset{0.0f, 0.0f};
        Vector2 uvScale{1.0f, 1.0f};
    };

    // A run of screen quads sharing one shader and texture. Binds and uploads
    // the mapping uniforms once, then re-sends only what varies per quad. The
    // debug panel draws a few thousand 8x8 glyphs a frame, where that per-draw
    // uniform traffic, not the geometry, is the cost.
    void DrawScreenQuadBatch(const ScreenQuad* quads, size_t count, Shader* shader, Texture* texture = nullptr);

    Shader* GetDefaultShader() const { return m_DefaultShader.get();}

    // Global chunky-pixel-art scale: how many window pixels one game pixel
    // renders as. Applies to every WORLD-space draw -- there is one Renderer2D
    // for the whole engine, so it is global with no per-actor bookkeeping.
    // Screen-space draws are exempt, so UI stays crisp at native resolution
    // however chunky the world gets.
    //
    // Implemented as a divisor on the effective u_ViewportSize, which is
    // mathematically the same as zooming the camera but exposed as a plain
    // "N pixels per game pixel" dial. It does not touch the content rect
    // SetActiveCamera computes -- only how many world units pack into it -- so
    // it composes with aspect-fitting instead of fighting it.
    //
    // 1.0 is an exact no-op; <= 0 clamps back to 1.0.
    void SetPixelScale(float scale);
    float GetPixelScale() const { return m_PixelScale; }

private:
    // world = true uses the active camera; world = false always passes the
    // identity mapping. One helper backs both draw families, so only these two
    // call sites decide which mapping applies.
    void ApplyCommonUniforms(Shader& shader, const Transform2D& transform, const Vector2& size,
                              const Color& color, bool world) const;

    void SubmitQuad(Shader& active);

    // Which region of the window the GL viewport currently covers. World draws
    // want Content (the letterboxed camera rect), screen draws want FullWindow.
    // Tracked so a run of same-mode draws -- the common case -- doesn't re-issue
    // a redundant glViewport, in the same spirit as Shader::Bind()'s check.
    enum class ViewportMode { FullWindow, Content };
    void EnsureViewport(ViewportMode mode);

    unsigned int m_VBO = 0;
    std::unique_ptr<Shader> m_DefaultShader;

    // The quad VBO is the only array buffer in the engine, so it stays bound
    // from Init() and the vertex attrib is only re-pointed when a shader puts
    // a_LocalPos at a different index. -1 means nothing is enabled yet.
    int m_ActiveAttrib = -1;

    float m_Width = 1.0f;
    float m_Height = 1.0f;
    float m_Time = 0.0f;

    bool m_HasCamera = false;
    Vector2 m_CameraPos;
    Vector2 m_CameraViewport;

    // The camera's content rect in real window pixels, top-left origin.
    // Centred letterboxing makes the top and bottom margins equal, so GL's
    // bottom-left convention needs no Y-flip here. Only meaningful while
    // m_HasCamera is true.
    float m_ContentX = 0.0f, m_ContentY = 0.0f;
    float m_ContentW = 1.0f, m_ContentH = 1.0f;

    ViewportMode m_ViewportMode = ViewportMode::FullWindow;

    bool m_Initialized = false;
    float m_PixelScale = 1.0f;
};
