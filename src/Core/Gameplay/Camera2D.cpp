#include "Camera2D.h"
#include "../Physics/RigidBody2D.h"
#include <algorithm>
#include <cmath>

void Camera2D::Follow(RigidBody2D& selfBody, float dt) {
    if (!followTarget || followSmoothing <= 0.0f) return;

    // Exponential decay toward the target, framerate-independent: at
    // followSmoothing = 5, roughly 63% of the remaining distance closes
    // per second regardless of dt/framerate (same shape as a physical
    // spring-damper snapping to a target, minus the overshoot). Clamped to
    // 1 so a huge dt spike (a debugger pause, a hitch) snaps instead of
    // overshooting past the target and oscillating.
    float t = 1.0f - std::exp(-followSmoothing * dt);
    t = std::clamp(t, 0.0f, 1.0f);

    // focusOffset shifts what "centered" even means (e.g. framed above
    // the player) -- added to the raw target position before the lerp,
    // so the camera eases toward the OFFSET point, not the target itself.
    Vector2 focusPoint = followTarget->transform.position + focusOffset;

    selfBody.transform.position = Vector2::Lerp(
        selfBody.transform.position, focusPoint, t);
}