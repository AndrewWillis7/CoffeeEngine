#pragma once
#include "Core/Math/Color.h"
#include "Core/Math/Vector2.h"

class ActorRegistry;
class RigidBody2D;
class Renderer2D;

// World-space gizmos: collider outlines, light reach, velocity vectors and the
// terrain heightfield, drawn after the scene and before the panel so they sit
// over the game but under the UI.
//
// Everything is built from thin rotated quads through Renderer2D::DrawQuad --
// the renderer has no line primitive to borrow -- which also means the gizmos
// go through the active camera and stay pinned to the world as it pans.
class DebugOverlay {
public:
    struct Options {
        bool bodyBounds = false;
        bool colliders = false;
        bool lightRanges = false;
        bool velocities = false;
        bool terrainSurface = false;
    };

    Options options;

    // `time` drives the selection pulse only; pass the unscaled clock so the
    // highlight keeps breathing while the scene is paused.
    void Draw(Renderer2D& renderer, ActorRegistry& actors, const RigidBody2D* selected, float time);

private:
    static void Line(Renderer2D& renderer, const Vector2& from, const Vector2& to,
                     float thickness, const Color& color);
    static void RectOutline(Renderer2D& renderer, const Vector2& center, const Vector2& size,
                            float thickness, const Color& color);
    static void CircleOutline(Renderer2D& renderer, const Vector2& center, float radius,
                              int segments, float thickness, const Color& color);
};
