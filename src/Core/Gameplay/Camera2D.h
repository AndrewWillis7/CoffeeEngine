#pragma once
#include "../Math/Vector2.h"

class RigidBody2D;

// Marks a RigidBody2D as a camera; owned by ActorRegistry, attached via a
// non-owning RigidBody2D::camera pointer like every other capability tag.
//
// Deliberately has no position of its own -- the owning body's
// transform.position IS the camera's world-space center.
class Camera2D {
public:
    // World units visible across the full window, independent of the window's
    // real pixel resolution: this is the resolution knob. A small viewport
    // (320x180) makes every world texel draw several screen pixels wide. X and Y
    // are independent, so a viewport whose aspect doesn't match the window's
    // stretches -- matching it (or updating on resize) is the caller's job.
    Vector2 viewportSize = Vector2(320.0f, 180.0f);

    // Optional second aspect constraint. Zero (either component) means unset:
    // content is fit against the real window using viewportSize's own aspect.
    // Set both (SetTargetAspect(16, 9)) to force output into a region of that
    // shape instead -- a 1:1 viewport at 16:9 renders square, pillarboxed inside
    // a 16:9 rect, itself letterboxed against the window. Renderer2D does this as
    // one nested fit, never a non-uniform stretch.
    Vector2 targetAspect = Vector2::Zero();

    // Non-owning, set from Lua via camera:SetFollowTarget(body).
    RigidBody2D* followTarget = nullptr;

    // World-space offset from followTarget that the camera actually leans
    // toward: Vector2(0, -40) frames 40px above the player (+Y is down). Only
    // applies through Follow().
    Vector2 focusOffset = Vector2::Zero();

    // Exponential-decay rate per second, NOT a 0..1 blend factor -- that is what
    // keeps Follow() framerate-independent. 0 disables it entirely, for a camera
    // body you drive by hand.
    float followSmoothing = 5.0f;

    // Only one camera drives rendering at a time; GetActiveCamera() returns the
    // first body whose Camera2D has this set.
    bool active = true;

    // Integer zoom layered on top of viewportSize rather than replacing it, so
    // viewportSize stays the native texel resolution the art was authored at and
    // gameplay code never has to restore a "base" value. 1 shows viewportSize
    // texels; 2 shows twice as much world, so each texel draws half size.
    //
    // Whole numbers only, deliberately: Renderer2D letterboxes a fixed content
    // rect, so a fractional zoom would land between texels and some would take a
    // different fraction of a screen pixel than their neighbours -- uneven,
    // blurry pixel art. Clamped to >= 1 so a stray 0 can't collapse the framing.
    int GetZoomOut() const { return m_ZoomOut; }
    void SetZoomOut(int zoom) { m_ZoomOut = zoom > 0 ? zoom : 1; }

    // What's visible right now -- read this rather than viewportSize directly.
    Vector2 EffectiveViewportSize() const {
        return viewportSize * static_cast<float>(m_ZoomOut);
    }

    // Eases `selfBody` toward followTarget + focusOffset. Out-of-line because it
    // needs RigidBody2D's full definition; RigidBody2D::UpdateCamera forwards here.
    void Follow(RigidBody2D& selfBody, float dt);

private:
    int m_ZoomOut = 1;
};
