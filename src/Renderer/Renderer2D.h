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
    // cleared again) -- position is the world-space point mapped to the
    // center of the screen, viewportSize is how many world units span the
    // full window regardless of its actual pixel size (see Camera2D's
    // header for why that's the "resolution control" knob). Screen-space
    // draws (DrawScreenQuad/DrawScreenTexturedQuad) are never affected by
    // this -- that's the whole point of the split, see those methods.
    void SetActiveCamera(const Vector2& position, const Vector2& viewportSize);

    // Reverts world-space draws to the legacy identity mapping (world
    // pixels == screen pixels, origin top-left) -- i.e. "no camera".
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

    unsigned int m_VBO = 0;
    std::unique_ptr<Shader> m_DefaultShader;

    float m_Width = 1.0f;
    float m_Height = 1.0f;
    float m_Time = 0.0f;

    bool m_HasCamera = false;
    Vector2 m_CameraPos;
    Vector2 m_CameraViewport;

    bool m_Initialized = false;
};