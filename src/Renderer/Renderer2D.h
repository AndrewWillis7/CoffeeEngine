#pragma once
#include "Core/Math/Transform2D.h"
#include "Core/Math/Color.h"
#include "Texture.h"
#include <memory>

class Shader;

// The Real Drawing Pipeline, Shader Based, VBO backend.
// Deliberately knows nothing on the OS side

// THIS DOES ASSUME A GL CONTEXT IS CREATED!!!!

class Renderer2D {
public:
    Renderer2D();
    ~Renderer2D();

    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    // Call once after the GL context is current
    void Init();
    void Shutdown();

    // Call at startup and whenever the window resizes
    void SetViewportSize(int width, int height);

    // Call once per frame before any Draw calls
    void BeginFrame(float deltaTime);

    // Sets the camera that world-space draws (DrawQuad/DrawTexturedQuad,
    // below) map against for the rest of this frame (or until changed/
    // cleared again).
    //   position     -- world-space point mapped to the center of the
    //                    camera's own content rect (see below).
    //   viewportSize -- how many world units span that content rect,
    //                    regardless of its actual pixel size (see
    //                    Camera2D's header for why that's the
    //                    "resolution control" knob).
    //   targetAspect -- optional (pass Vector2::Zero() for "unset").
    //                    Forces the content rect into a specific on-screen
    //                    SHAPE (e.g. 16:9) rather than just viewportSize's
    //                    own aspect -- see Camera2D::targetAspect.
    //   borderShader -- drawn as a single full-window quad BEFORE the GL
    //                    viewport narrows down to the (possibly smaller,
    //                    letterboxed) content rect, so it's what shows
    //                    through in the margins. May be nullptr (no
    //                    border draw, margins just show whatever glClear
    //                    left behind -- matches the old behavior).
    //   borderTexture -- optional. When set (and borderShader is valid),
    //                    the border is drawn via DrawScreenTexturedQuad
    //                    instead of DrawScreenQuad, so a PixelSprite can
    //                    back the border (see ActorRegistry::SetBorderSprite).
    //                    May be nullptr (flat-color/procedural border only).
    //
    // The content rect is computed via a nested aspect-fit (see the
    // FitAspect helper in the .cpp): fit targetAspect (or, if unset,
    // viewportSize) into the real window preserving aspect (uniform
    // scale, so it never stretches), then fit viewportSize into THAT
    // rect the same way. Either step degenerates to "fill exactly" when
    // the aspects already match, so this same call handles every case
    // (no targetAspect, matching targetAspect, mismatched targetAspect)
    // without a branch. Screen-space draws (DrawScreenQuad/
    // DrawScreenTexturedQuad) are never affected by any of this -- that's
    // the whole point of the split, see those methods.
    void SetActiveCamera(const Vector2& position, const Vector2& viewportSize,
                          const Vector2& targetAspect, Shader* borderShader,
                          Texture* borderTexture = nullptr);

    // Reverts world-space draws to the legacy identity mapping (world
    // pixels == screen pixels, full window, origin top-left) -- i.e. "no
    // camera". Also drops the GL viewport back to the full window.
    void ClearActiveCamera();

    // Basic Quad Draw at Transform, can have applied shaders. World-space:
    // mapped through the active camera if one is set (see SetActiveCamera),
    // otherwise falls back to the identity mapping -- so any script that
    // never creates a camera draws exactly as before.
    void DrawQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader);

    void DrawTexturedQuad(const Transform2D& transform, const Vector2& size, const Color& tint,
                        Shader* shader, Texture* texture,
                        Vector2 uvOffset = {0.0f, 0.0f}, Vector2 uvScale = {1.0f, 1.0f});

    // Screen-space equivalents of the two above -- ALWAYS map 1:1 against
    // the real window resolution, ignoring whatever camera is currently
    // active. Used by the engine debug UI (UIPanel) so it stays glued to
    // the screen instead of panning/zooming with the game's camera.
    void DrawScreenQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader);

    void DrawScreenTexturedQuad(const Transform2D& transform, const Vector2& size, const Color& tint,
                        Shader* shader, Texture* texture,
                        Vector2 uvOffset = {0.0f, 0.0f}, Vector2 uvScale = {1.0f, 1.0f});

    Shader* GetDefaultShader() const { return m_DefaultShader.get();}

    // Global "chunky pixel art" scale -- how many real/window pixels one
    // game pixel (one PixelSprite texel, or one world unit for a flat-
    // color body -- the two are the same thing in this engine, see
    // RigidBody2D::SetSprite) renders as. Applies uniformly to every
    // WORLD-space draw (DrawQuad/DrawTexturedQuad) regardless of which
    // actor, sprite, or shader is doing the drawing -- there's exactly
    // one Renderer2D for the whole engine, so this is automatically
    // "global to all actors" the moment it's set, with no per-actor
    // bookkeeping needed. Screen-space draws (DrawScreenQuad/
    // DrawScreenTexturedQuad -- the debug UI panel) are deliberately
    // EXEMPT, same "UI shouldn't be forced onto the game's pixel grid"
    // reasoning ApplyCommonUniforms' u_PixelSnap comment already uses --
    // UI stays crisp at native resolution no matter how chunky the game
    // world gets.
    //
    // Implemented as a divisor on the effective u_ViewportSize fed to the
    // vertex shader (see ApplyCommonUniforms) -- mathematically identical
    // to zooming the camera in, just exposed as a plain "N pixels per
    // game pixel" multiplier instead of asking a script to reason about
    // Camera2D::viewportSize's absolute world-unit count. Does NOT touch
    // the actual on-screen CONTENT RECT size/letterboxing computed by
    // SetActiveCamera -- only how many world units get packed into
    // whatever rect that already produced, so it composes cleanly with
    // camera aspect-fitting instead of fighting it.
    //
    // 1.0 (the default) is an exact no-op; invalid input (<= 0) is
    // clamped back to 1.0 rather than risking a divide-by-zero/negative
    // viewport, same defensive spirit SetActiveCamera already uses for a
    // zero/negative Camera2D::viewportSize.
    void SetPixelScale(float scale);
    float GetPixelScale() const { return m_PixelScale; }

private:
    // world: true uses the active camera (if any) via ApplyCommonUniforms'
    // cameraPos/viewportSize args; world: false (screen-space) always
    // passes the identity mapping (window center / full resolution)
    // regardless of what SetActiveCamera set. Same helper backs both the
    // World* and Screen* draw calls above -- only these two call sites
    // decide which mapping applies.
    void ApplyCommonUniforms(Shader& shader, const Transform2D& transform, const Vector2& size,
                              const Color& color, bool world) const;

    void SubmitQuad(Shader& active);

    // Which region of the real window the GL viewport is currently set
    // to. World-space draws need Content (the letterboxed camera rect, or
    // the full window if no camera/no letterboxing); screen-space draws
    // always need FullWindow. Tracked so repeated same-mode draws (the
    // overwhelmingly common case -- a frame draws many quads in a row
    // without switching) don't re-issue a redundant glViewport() call
    // each time; same "avoid redundant GL state changes" spirit as
    // Shader::Bind()'s s_CurrentProgram check.
    enum class ViewportMode { FullWindow, Content };
    void EnsureViewport(ViewportMode mode);

    unsigned int m_VBO = 0;
    std::unique_ptr<Shader> m_DefaultShader;

    float m_Width = 1.0f;
    float m_Height = 1.0f;
    float m_Time = 0.0f;

    bool m_HasCamera = false;
    Vector2 m_CameraPos;
    Vector2 m_CameraViewport;

    // The camera's on-screen content rect, in real window pixels
    // (top-left origin -- see SetActiveCamera's .cpp for why no Y-flip is
    // needed for glViewport despite GL's bottom-left convention: centered
    // letterboxing makes the top-margin and bottom-margin equal, so the
    // ambiguity is moot). Recomputed every SetActiveCamera() call; only
    // meaningful while m_HasCamera is true.
    float m_ContentX = 0.0f, m_ContentY = 0.0f;
    float m_ContentW = 1.0f, m_ContentH = 1.0f;

    ViewportMode m_ViewportMode = ViewportMode::FullWindow;

    bool m_Initialized = false;
    float m_PixelScale = 1.0f;
};