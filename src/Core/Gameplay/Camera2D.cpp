#include "Camera2D.h"
#include "../Physics/RigidBody2D.h"
#include <algorithm>
#include <cmath>

void Camera2D::Follow(RigidBody2D& selfBody, float dt) {
    if (!followTarget || followSmoothing <= 0.0f) return;

    // Framerate-independent exponential decay: at followSmoothing = 5, ~63% of
    // the remaining distance closes per second regardless of dt. Clamped so a
    // large dt spike snaps rather than overshooting into oscillation.
    float t = 1.0f - std::exp(-followSmoothing * dt);
    t = std::clamp(t, 0.0f, 1.0f);

    // Eases toward the OFFSET point, not the target itself.
    Vector2 focusPoint = followTarget->transform.position + focusOffset;

    selfBody.transform.position = Vector2::Lerp(
        selfBody.transform.position, focusPoint, t);
}
