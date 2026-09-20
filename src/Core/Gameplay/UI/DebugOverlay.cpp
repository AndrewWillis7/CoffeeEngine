#include "DebugOverlay.h"
#include "UIStyle.h"
#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Physics/CollisionShape2D.h"
#include "Core/Gameplay/LightEmitterConfig.h"
#include "Core/Gameplay/Terrain/TerrainChunk.h"
#include "Renderer/Renderer2D.h"
#include <cmath>

namespace {

// World units. One texel reads as a hairline at the native 320x180 resolution
// and scales up with everything else, which is what a pixel-art gizmo wants.
constexpr float kThin = 1.0f;

// Velocity is in units per second; a tenth of a second of travel keeps the
// arrow readable at walking speed without running off screen at terminal
// velocity.
constexpr float kVelocityScale = 0.1f;

Color Fade(const Color& color, float alpha) {
    return {color.r, color.g, color.b, color.a * alpha};
}

} // namespace

void DebugOverlay::Line(Renderer2D& renderer, const Vector2& from, const Vector2& to,
                        float thickness, const Color& color) {
    const Vector2 delta = to - from;
    const float length = delta.Length();
    if (length <= 0.0001f) return;

    Transform2D transform;
    transform.position = {(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f};
    transform.rotation = std::atan2(delta.y, delta.x);
    renderer.DrawQuad(transform, {length, thickness}, color, nullptr);
}

void DebugOverlay::RectOutline(Renderer2D& renderer, const Vector2& center, const Vector2& size,
                               float thickness, const Color& color) {
    const float halfW = size.x * 0.5f;
    const float halfH = size.y * 0.5f;

    // Horizontals run the full width and verticals the full height, so the
    // corners overlap rather than leaving a notch.
    Transform2D transform;
    transform.position = {center.x, center.y - halfH};
    renderer.DrawQuad(transform, {size.x, thickness}, color, nullptr);
    transform.position = {center.x, center.y + halfH};
    renderer.DrawQuad(transform, {size.x, thickness}, color, nullptr);
    transform.position = {center.x - halfW, center.y};
    renderer.DrawQuad(transform, {thickness, size.y}, color, nullptr);
    transform.position = {center.x + halfW, center.y};
    renderer.DrawQuad(transform, {thickness, size.y}, color, nullptr);
}

void DebugOverlay::CircleOutline(Renderer2D& renderer, const Vector2& center, float radius,
                                 int segments, float thickness, const Color& color) {
    if (radius <= 0.0f || segments < 3) return;

    const float step = 6.28318530718f / static_cast<float>(segments);
    Vector2 previous{center.x + radius, center.y};
    for (int i = 1; i <= segments; ++i) {
        const float angle = step * static_cast<float>(i);
        const Vector2 current{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
        Line(renderer, previous, current, thickness, color);
        previous = current;
    }
}

void DebugOverlay::Draw(Renderer2D& renderer, ActorRegistry& actors,
                        const RigidBody2D* selected, float time) {
    for (const auto& owned : actors.GetBodies()) {
        const RigidBody2D& body = *owned;

        if (options.bodyBounds) {
            RectOutline(renderer, body.transform.position,
                        {body.size.x * body.transform.scale.x, body.size.y * body.transform.scale.y},
                        kThin, UITheme::GizmoBounds);
        }

        if (options.colliders && body.collisionShape) {
            const CollisionShape2D& shape = *body.collisionShape;
            const Vector2 center = body.transform.position + shape.GetOffset();
            if (shape.GetType() == CollisionShape2D::Type::Box) {
                RectOutline(renderer, center, shape.GetHalfExtents() * 2.0f, kThin, UITheme::GizmoCollider);
            } else {
                CircleOutline(renderer, center, shape.GetRadius(), 24, kThin, UITheme::GizmoCollider);
            }
        }

        if (options.lightRanges && body.lightEmitter) {
            const LightEmitterConfig& light = *body.lightEmitter;
            const Vector2 origin = body.transform.position;
            const Color tint = Fade(light.color, 0.5f);

            CircleOutline(renderer, origin, light.radius, 32, kThin, tint);

            if (light.type == LightEmitterConfig::Type::Cone) {
                const float aim = light.coneDirectionRad + (light.useOwnerRotation ? body.transform.rotation : 0.0f);
                const float half = light.coneAngleRad * 0.5f;
                for (float edge : {aim - half, aim + half}) {
                    Line(renderer, origin,
                         {origin.x + std::cos(edge) * light.radius, origin.y + std::sin(edge) * light.radius},
                         kThin, tint);
                }
            }
        }

        if (options.velocities && body.velocity.LengthSquared() > 1.0f) {
            Line(renderer, body.transform.position,
                 body.transform.position + body.velocity * kVelocityScale,
                 kThin, UITheme::GizmoVelocity);
        }

        if (options.terrainSurface && body.terrain && body.terrain->IsGenerated()) {
            // Every fourth column: the heightmap is snapped to whole texels, so
            // a denser sample only redraws the same steps.
            const float halfWidth = body.size.x * 0.5f * body.transform.scale.x;
            const float left = body.transform.position.x - halfWidth;
            const float right = body.transform.position.x + halfWidth;
            const float step = 4.0f;

            Vector2 previous{left, body.terrain->SurfaceWorldY(left, body)};
            for (float x = left + step; x <= right; x += step) {
                const Vector2 current{x, body.terrain->SurfaceWorldY(x, body)};
                Line(renderer, previous, current, kThin, UITheme::GizmoSurface);
                previous = current;
            }
        }
    }

    if (!selected) return;

    // A slow breath rather than a blink: visible against both the lit and
    // unlit halves of the scene without becoming the brightest thing on screen.
    const float pulse = 0.55f + 0.45f * std::sin(time * 4.0f);
    const Vector2 size{selected->size.x * selected->transform.scale.x,
                       selected->size.y * selected->transform.scale.y};

    RectOutline(renderer, selected->transform.position, size + Vector2{2.0f, 2.0f}, kThin,
                Fade(UITheme::GizmoSelected, pulse));

    // Crosshair arms reaching out of the box, so a selected body is findable
    // even when it is one texel wide.
    const Vector2 center = selected->transform.position;
    const float reach = std::max(size.x, size.y) * 0.5f + 10.0f;
    const Color arm = Fade(UITheme::GizmoSelected, pulse * 0.6f);
    Line(renderer, {center.x - reach, center.y}, {center.x - size.x * 0.5f - 2.0f, center.y}, kThin, arm);
    Line(renderer, {center.x + size.x * 0.5f + 2.0f, center.y}, {center.x + reach, center.y}, kThin, arm);
    Line(renderer, {center.x, center.y - reach}, {center.x, center.y - size.y * 0.5f - 2.0f}, kThin, arm);
    Line(renderer, {center.x, center.y + size.y * 0.5f + 2.0f}, {center.x, center.y + reach}, kThin, arm);
}
