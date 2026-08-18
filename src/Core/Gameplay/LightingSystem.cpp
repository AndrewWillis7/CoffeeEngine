#include "LightingSystem.h"
#include "LightEmitterConfig.h"
#include "../ActorRegistry.h"
#include "../Physics/RigidBody2D.h"
#include "../Math/AABB.h"
#include "../../Renderer/PixelSprite.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

// Ray-march tuning -- see LightingSystem.h's PERFORMANCE NOTE. Both are
// plain constants for now rather than LightEmitterConfig fields: they're
// about HOW the system samples the world, not what any one light looks
// like, so a single engine-wide value is the right knob until profiling
// says otherwise.
//
// kStepPixels is a texel count, not a fraction -- so it needs to track
// how fine-grained the content actually is. At 2.0 it only samples ~8
// columns across a 16-texel-wide sprite per ray, which reads as visibly
// gappy/stair-stepped lighting on something that small (it was fine
// against the old 50-texel-wide placeholder player). 1.0 keeps every
// texel column reachable by at least one ray without doubling the
// engine's actual bottleneck (candidate-body count, still O(n) with no
// broad-phase index) -- see the "on the horizon" list.
constexpr float kStepPixels = 1.0f;          // world-pixel distance advanced per ray-march step
constexpr float kRayDegreesPerSample = 2.0f; // target angular resolution
constexpr int kMinRays = 12;
constexpr int kMaxRays = 220; // caps a full 360-degree Point light at ~1.6 deg resolution

// Inverse of the vertex shader's local->world quad transform (see
// scripts/shaders/quad.vert and Renderer2D::ApplyCommonUniforms) -- maps
// a WORLD point onto the pixel coordinates of `body`'s attached sprite.
// Returns false (leaving outX/outY unset) if the body has no sprite or
// the point falls outside its bounds.
bool WorldToPixel(const RigidBody2D& body, const Vector2& worldPoint, int& outX, int& outY) {
    if (!body.sprite) return false;

    Vector2 d = worldPoint - body.transform.position;
    Vector2 local = d.Rotated(-body.transform.rotation); // undo the body's rotation

    float sx = body.transform.scale.x != 0.0f ? body.transform.scale.x : 1.0f;
    float sy = body.transform.scale.y != 0.0f ? body.transform.scale.y : 1.0f;
    local.x /= sx; // undo the body's scale
    local.y /= sy;

    float px = local.x + body.size.x * 0.5f;
    float py = local.y + body.size.y * 0.5f;
    outX = static_cast<int>(std::floor(px));
    outY = static_cast<int>(std::floor(py));

    return outX >= 0 && outY >= 0 && outX < body.sprite->GetWidth() && outY < body.sprite->GetHeight();
}

// Conservative world-space AABB for a sprite-backed body's VISUAL extent
// (size * scale, centered on transform.position) -- deliberately NOT
// CollisionShape2D::GetWorldAABB (which can be smaller, larger, offset,
// or simply absent -- e.g. an ArtObject has no collision shape at all,
// but still wants to catch light). Padded out to the diagonal so a
// rotated sprite's true footprint is never underestimated by this
// broad-phase check -- same "conservative box, cheap to test" spirit
// AABB.h already uses for collision.
AABB SpriteWorldAABB(const RigidBody2D& body) {
    Vector2 half = (body.size * body.transform.scale) * 0.5f;
    float diag = half.Length();
    return AABB{body.transform.position, Vector2(diag, diag)};
}

float Smoothstep01(float edge0, float edge1, float x) {
    if (edge0 == edge1) return x < edge0 ? 0.0f : 1.0f;
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Two non-harmonic sine waves summed and clamped to roughly [-1, 1] --
// reads as organic/irregular flicker instead of a single perfect sine's
// metronome ticking. `seed` (derived per-light from its own config
// pointer) keeps multiple torches from flickering in lockstep.
float FlickerFactor(float time, float speed, float seed) {
    float t = time * speed;
    float wobble = std::sin(t + seed * 6.2831853f) * 0.6f
                 + std::sin(t * 2.37f + seed * 11.0f) * 0.4f;
    return std::clamp(wobble, -1.0f, 1.0f);
}

struct LightSample {
    RigidBody2D* body = nullptr;
    LightEmitterConfig* config = nullptr;
};

} // namespace

void LightingSystem::Update(ActorRegistry& actors, float deltaTime) {
    m_Time += deltaTime;

    const auto& bodies = actors.GetBodies();

    // Pass 0: find every active light.
    std::vector<LightSample> lights;
    for (const auto& bodyPtr : bodies) {
        RigidBody2D* body = bodyPtr.get();
        if (body->lightEmitter) lights.push_back({body, body->lightEmitter});
    }

    // Pass 1: erase whatever LAST frame's lights touched, BEFORE this
    // frame re-accumulates anything -- so a light that moved (or was
    // removed/unset since last frame) doesn't leave a stale tinted patch
    // sitting on a wall behind it.
    for (const LitRect& rect : m_PrevLitRects) {
        if (rect.body->sprite) rect.body->sprite->ResetLightingRect(rect.minX, rect.minY, rect.maxX, rect.maxY);
    }

    if (lights.empty()) {
        m_PrevLitRects.clear();
        return;
    }

    std::vector<LitRect> newLitRects;

    // Pass 2: cast every light.
    for (const LightSample& light : lights) {
        RigidBody2D* lightBody = light.body;
        LightEmitterConfig* cfg = light.config;
        if (cfg->radius <= 0.0f) continue;

        // Per-light flicker sample for this frame -- computed once, not
        // per-ray/per-pixel, so one frame's lit region never has a seam
        // from its own flicker (see LightEmitterConfig.h).
        float seed = static_cast<float>(reinterpret_cast<uintptr_t>(cfg) % 10007) / 10007.0f;
        float flicker = cfg->flicker ? FlickerFactor(m_Time, cfg->flickerSpeed, seed) : 0.0f;
        float brightness = std::max(0.0f, cfg->brightness * (1.0f + flicker * cfg->flickerIntensityAmount));

        Color color = cfg->color;
        if (cfg->flicker) {
            float shift = std::max(0.0f, flicker); // only warms up on the flicker's upswing
            color.r += cfg->flickerColorShift.r * shift;
            color.g += cfg->flickerColorShift.g * shift;
            color.b += cfg->flickerColorShift.b * shift;
        }
        if (brightness <= 0.0f) continue;

        Vector2 lightPos = lightBody->transform.position;

        // Broad-phase: which sprite-backed bodies are even worth
        // ray-testing for this light -- see the PERFORMANCE NOTE in
        // LightingSystem.h.
        std::vector<RigidBody2D*> candidates;
        for (const auto& bodyPtr : bodies) {
            RigidBody2D* candidate = bodyPtr.get();
            if (!candidate->sprite) continue;
            AABB box = SpriteWorldAABB(*candidate);
            float reach = cfg->radius + box.halfExtents.x; // box is a (diag,diag) square, either axis works
            if (Vector2::Distance(lightPos, box.center) > reach) continue;
            candidates.push_back(candidate);
        }
        if (candidates.empty()) continue;

        // Angle range to sweep this light across: a full circle for
        // Point, a centered arc for Cone. coneDirectionRad is optionally
        // relative to the owning body's own rotation (see
        // LightEmitterConfig::useOwnerRotation).
        bool isCone = cfg->type == LightEmitterConfig::Type::Cone;
        float startDeg, sweepDeg;
        if (isCone) {
            float dirDeg = cfg->coneDirectionRad * kRadToDeg;
            if (cfg->useOwnerRotation) dirDeg += lightBody->transform.rotation * kRadToDeg;
            sweepDeg = std::max(0.0f, cfg->coneAngleRad * kRadToDeg);
            startDeg = dirDeg - sweepDeg * 0.5f;
        } else {
            startDeg = 0.0f;
            sweepDeg = 360.0f;
        }
        if (sweepDeg <= 0.0f) continue;

        int rayCount = std::clamp(static_cast<int>(sweepDeg / kRayDegreesPerSample), kMinRays, kMaxRays);
        // Cone rays cover BOTH edges inclusive (a sharp, visible cone
        // boundary); a full-circle Point sweep must NOT double-cover the
        // 0/360 seam, so it steps by sweepDeg/rayCount instead.
        float angleStep = (isCone && rayCount > 1) ? sweepDeg / static_cast<float>(rayCount - 1)
                                                     : sweepDeg / static_cast<float>(rayCount);

        // Per-light accumulation of "which body, what pixel rect did THIS
        // light touch" -- flushed into newLitRects once this light's
        // rays are all done.
        std::unordered_map<RigidBody2D*, LitRect> touched;

        for (int i = 0; i < rayCount; ++i) {
            float angleDeg = startDeg + angleStep * static_cast<float>(i);

            // Soft cone edge: full strength across the inner 80% of the
            // arc, smoothly fading to zero at the boundary, so a
            // spotlight doesn't have a razor-sharp cutoff. No-op (always
            // 1.0) for Point lights.
            float angularFactor = 1.0f;
            if (isCone) {
                float distFromCenterDeg = std::abs(angleDeg - (startDeg + sweepDeg * 0.5f));
                float halfAngle = sweepDeg * 0.5f;
                angularFactor = 1.0f - Smoothstep01(halfAngle * 0.8f, halfAngle, distFromCenterDeg);
            }
            if (angularFactor <= 0.0f) continue;

            float angleRad = angleDeg * kDegToRad;
            Vector2 dir(std::cos(angleRad), std::sin(angleRad));

            for (float traveled = 0.0f; traveled <= cfg->radius; traveled += kStepPixels) {
                Vector2 sample = lightPos + dir * traveled;

                RigidBody2D* hitBody = nullptr;
                int hitX = 0, hitY = 0;
                bool hitBlocks = false;

                // First solid hit wins -- the engine has no z-order/
                // layering concept yet (bodies just draw in whatever
                // order Draw() calls happen), so with overlapping sprites
                // this is an arbitrary-but-stable pick, not a "nearest to
                // camera" one. Flagged as a known simplification.
                for (RigidBody2D* candidate : candidates) {
                    int px, py;
                    if (!WorldToPixel(*candidate, sample, px, py)) continue;
                    if (!candidate->sprite->IsSolid(px, py)) continue;
                    hitBody = candidate;
                    hitX = px; hitY = py;
                    hitBlocks = candidate->lightBlocking;
                    break;
                }

                if (hitBody) {
                    float falloffT = std::clamp(traveled / cfg->radius, 0.0f, 1.0f);
                    float strength = brightness * angularFactor * std::pow(1.0f - falloffT, cfg->falloffExponent);
                    if (strength > 0.0f) {
                        hitBody->sprite->AccumulateLightTint(hitX, hitY, color, strength);

                        auto it = touched.find(hitBody);
                        if (it == touched.end()) {
                            touched[hitBody] = LitRect{hitBody, hitX, hitY, hitX, hitY};
                        } else {
                            it->second.minX = std::min(it->second.minX, hitX);
                            it->second.minY = std::min(it->second.minY, hitY);
                            it->second.maxX = std::max(it->second.maxX, hitX);
                            it->second.maxY = std::max(it->second.maxY, hitY);
                        }
                    }
                    if (hitBlocks) break; // opaque -- ray stops here, casts a shadow behind it
                }
            }
        }

        for (auto& [body, rect] : touched) newLitRects.push_back(rect);
    }

    m_PrevLitRects = std::move(newLitRects);
}