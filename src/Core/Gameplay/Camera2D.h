#pragma once
#include "../Math/Vector2.h"

class RigidBody2D;

// Marks a RigidBody2D as a camera. Attached the same way Shader/
// CollisionShape2D/PlayerActorConfig are -- a non-owning pointer on the
// RigidBody2D (see RigidBody2D::camera), instance owned and kept alive by
// ActorRegistry.
//
// Deliberately has NO position field of its own: the owning body's
// transform.position IS the camera's world-space center. Move the body
// (SetPosition, physics, Follow() below, whatever) and the camera moves
// with it -- same "the RigidBody2D is the actor" convention the rest of
// the engine already uses for Shader/CollisionShape2D/PlayerActorConfig.
class Camera2D {
public:
    // World units visible across the full window, regardless of the
    // window's actual pixel resolution -- this IS the "resolution
    // control" knob. A small viewport (e.g. 320x180) makes every world
    // pixel draw several screen pixels wide, filling whatever size the
    // window happens to be with clean, uniformly-scaled pixel art.
    // Larger values zoom out (more world visible, each world pixel drawn
    // smaller); smaller values zoom in. X and Y are independent, so a
    // viewport whose aspect ratio doesn't match the window's will stretch
    // -- picking a viewport aspect that matches your target window (or
    // updating it on resize) is on the caller for now.
    Vector2 viewportSize = Vector2(320.0f, 180.0f);

    // Optional second aspect-ratio constraint, independent of viewportSize.
    // Zero (default, either component) means "not set" -- the camera's
    // content is fit directly against the real window using viewportSize's
    // own aspect, one level of letterbox/pillarbox sized to whatever the
    // window/monitor happens to be shaped like.
    //
    // Set both components (e.g. SetTargetAspect(16, 9)) to force the
    // camera's output to live inside a region of that SHAPE instead,
    // regardless of viewportSize's own aspect -- e.g. a 1:1 50x50
    // viewportSize with a 16:9 targetAspect renders as a square, pillar-
    // boxed inside a 16:9 rectangle, which is itself letterboxed/
    // pillarboxed against the real window if the window's own aspect
    // doesn't match 16:9 either. Renderer2D does this as a single nested
    // fit (see its FitAspect helper) -- never a non-uniform stretch on
    // either level.
    Vector2 targetAspect = Vector2::Zero();

    // Non-owning. Set via Lua's camera:SetFollowTarget(body) (nil to stop
    // following). Same lifetime convention as every other cross-reference
    // in this engine (RigidBody2D::shader, etc.) -- owned and kept alive
    // elsewhere (ActorRegistry), this is just a pointer.
    RigidBody2D* followTarget = nullptr;

    // Exponential-decay follow rate (per second), NOT a 0..1 blend factor
    // -- this keeps Follow() frame-rate independent (see the .cpp). 0 means
    // "don't move on your own" (Follow() becomes a no-op; drive the body's
    // position by hand instead, e.g. an editor-style scroll camera).
    // Higher values catch up to the target faster; very large values
    // approach an instant snap.
    float followSmoothing = 5.0f;

    // Only one camera actually drives rendering at a time -- same
    // "current" convention Godot's Camera2D uses. ActorRegistry::
    // GetActiveCamera() returns the first body whose attached Camera2D
    // has active == true.
    bool active = true;

    // Moves `selfBody` (the RigidBody2D this Camera2D is attached to)
    // toward followTarget's position by followSmoothing, scaled by dt.
    // No-op if followTarget is null or followSmoothing <= 0. See
    // RigidBody2D::UpdateCamera(), which just forwards into this --
    // exists here (not inline) because it needs RigidBody2D's full
    // definition (transform.position) which this header only forward-
    // declares.
    void Follow(RigidBody2D& selfBody, float dt);
};