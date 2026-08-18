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
constexpr float kRadToDeg = 180.0f / kPi;

// Occlusion-march tuning. This is now the ONLY thing that steps through
// world space piece by piece -- see LightingSystem.h's header comment
// for why "which pixels get lit" is no longer a ray sweep. It only needs
// to be fine enough that a thin wall can't be stepped clean over; unlike
// the old kStepPixels, it no longer has anything to do with how gapless
// the lighting itself looks.
constexpr float kOcclusionStepPixels = 1.0f;

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

// Exact inverse of WorldToPixel above -- given a pixel INDEX, returns
// that texel's CENTER in world space (the (px+0.5, py+0.5) mirrors
// WorldToPixel's floor(), so a world point that WorldToPixel maps to
// (px, py) sits within half a texel of what this returns for the same
// (px, py) -- close enough that distance/angle-to-light math on the
// result reads as "this pixel's position", not an edge or a corner).
// Every candidate-pixel lighting evaluation below is driven from THIS,
// not from a ray sample -- see LightingSystem.h's header comment.
Vector2 PixelToWorld(const RigidBody2D& body, int px, int py) {
    float sx = body.transform.scale.x != 0.0f ? body.transform.scale.x : 1.0f;
    float sy = body.transform.scale.y != 0.0f ? body.transform.scale.y : 1.0f;

    float localX = (static_cast<float>(px) + 0.5f) - body.size.x * 0.5f;
    float localY = (static_cast<float>(py) + 0.5f) - body.size.y * 0.5f;

    Vector2 local(localX * sx, localY * sy);       // redo the body's scale
    Vector2 rotated = local.Rotated(body.transform.rotation); // redo the body's rotation
    return body.transform.position + rotated;
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

// Normalizes a degree difference into (-180, 180] -- how far `angleDeg`
// sits from `centerDeg`, signed, shortest way around the circle.
float AngleDiffDeg(float angleDeg, float centerDeg) {
    float diff = angleDeg - centerDeg;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff <= -180.0f) diff += 360.0f;
    return diff;
}

struct LightSample {
    RigidBody2D* body = nullptr;
    LightEmitterConfig* config = nullptr;
};

// True if some lightBlocking body's solid pixel sits strictly between
// `from` (a light's world position) and `to` (a candidate pixel's world
// position). `skipBody` is the LIGHT's own owning body -- excluded so a
// body that both emits AND blocks (a lit lantern that's also solid,
// say) never shadows its own glow, per RigidBody2D::lightBlocking's own
// header comment ("blocking only affects OTHER lights' rays passing
// through this body, never its own glow").
//
// Deliberately NOT the target pixel's own body: a solid, blocking body
// SHOULD be able to shadow its own far side from an external light on
// its near side (e.g. campfire light hitting one face of the wall
// should not shine through the wall's own 16-texel bulk to light the
// opposite face) -- that's the actual point of marking something
// lightBlocking. Only the destination pixel's own immediate texel is
// protected from self-occlusion, via marchLimit below stopping the
// march a step short of `to`.
bool IsOccluded(const Vector2& from, const Vector2& to, const RigidBody2D* skipBody,
                 const std::vector<RigidBody2D*>& blockers) {
    if (blockers.empty()) return false;

    Vector2 delta = to - from;
    float dist = delta.Length();
    if (dist <= kOcclusionStepPixels) return false; // essentially at the light itself

    Vector2 dir = delta / dist;
    float marchLimit = dist - kOcclusionStepPixels;

    for (float traveled = kOcclusionStepPixels; traveled < marchLimit; traveled += kOcclusionStepPixels) {
        Vector2 sample = from + dir * traveled;
        for (RigidBody2D* blocker : blockers) {
            if (blocker == skipBody) continue;
            int px, py;
            if (!WorldToPixel(*blocker, sample, px, py)) continue;
            if (blocker->sprite->IsSolid(px, py)) return true;
        }
    }
    return false;
}

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

    // Gathered once per Update() call, not once per light -- which
    // bodies cast shadows doesn't depend on which light is asking, same
    // "collect once, reuse" spirit as each light's own candidate scan.
    std::vector<RigidBody2D*> blockers;
    for (const auto& bodyPtr : bodies) {
        RigidBody2D* body = bodyPtr.get();
        if (body->sprite && body->lightBlocking) blockers.push_back(body);
    }

    std::vector<LitRect> newLitRects;

    // Pass 2: light every candidate pixel of every candidate body, for
    // every light.
    for (const LightSample& light : lights) {
        RigidBody2D* lightBody = light.body;
        LightEmitterConfig* cfg = light.config;
        if (cfg->radius <= 0.0f) continue;

        // Per-light flicker sample for this frame -- computed once, not
        // per-pixel, so one frame's lit region never has a seam from its
        // own flicker (see LightEmitterConfig.h).
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
        // per-pixel testing for this light -- see the PERFORMANCE NOTE in
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

        // Cone-only aim: center direction in degrees, optionally riding
        // the owning body's own rotation (see LightEmitterConfig::
        // useOwnerRotation), and the full angular width of the wedge.
        bool isCone = cfg->type == LightEmitterConfig::Type::Cone;
        float dirDeg = 0.0f, sweepDeg = 360.0f;
        if (isCone) {
            dirDeg = cfg->coneDirectionRad * kRadToDeg;
            if (cfg->useOwnerRotation) dirDeg += lightBody->transform.rotation * kRadToDeg;
            sweepDeg = std::max(0.0f, cfg->coneAngleRad * kRadToDeg);
        }
        if (sweepDeg <= 0.0f) continue;
        float halfAngleDeg = sweepDeg * 0.5f;

        // Per-light accumulation of "which body, what pixel rect did THIS
        // light touch" -- flushed into newLitRects once this light is done.
        std::unordered_map<RigidBody2D*, LitRect> touched;

        for (RigidBody2D* candidate : candidates) {
            PixelSprite* sprite = candidate->sprite;
            int w = sprite->GetWidth();
            int h = sprite->GetHeight();

            for (int py = 0; py < h; ++py) {
                for (int px = 0; px < w; ++px) {
                    if (!sprite->IsSolid(px, py)) continue;

                    Vector2 worldPos = PixelToWorld(*candidate, px, py);
                    float dist = Vector2::Distance(lightPos, worldPos);
                    if (dist > cfg->radius) continue;

                    // Soft cone edge: full strength across the inner 80%
                    // of the arc, smoothly fading to zero at the
                    // boundary, so a spotlight doesn't have a razor-sharp
                    // cutoff. No-op (always 1.0) for Point lights.
                    float angularFactor = 1.0f;
                    if (isCone) {
                        Vector2 toPixel = worldPos - lightPos;
                        if (toPixel.LengthSquared() > 0.0001f) {
                            float angleDeg = std::atan2(toPixel.y, toPixel.x) * kRadToDeg;
                            float distFromCenterDeg = std::abs(AngleDiffDeg(angleDeg, dirDeg));
                            if (distFromCenterDeg > halfAngleDeg) continue; // outside the wedge entirely
                            angularFactor = 1.0f - Smoothstep01(halfAngleDeg * 0.8f, halfAngleDeg, distFromCenterDeg);
                        }
                        // else: pixel sits right on top of the light's
                        // own position -- direction is undefined, treat
                        // as dead-center (angularFactor stays 1.0).
                    }
                    if (angularFactor <= 0.0f) continue;

                    // Shadow test -- see IsOccluded's comment. Only now,
                    // after the cheap distance/angle checks above already
                    // ruled most pixels out, because this is the
                    // expensive part.
                    if (IsOccluded(lightPos, worldPos, lightBody, blockers)) continue;

                    float falloffT = std::clamp(dist / cfg->radius, 0.0f, 1.0f);
                    if (cfg->toneSteps > 0) {
                        // Round DOWN to the nearest 1/toneSteps -- turns
                        // the smooth gradient into toneSteps flat
                        // concentric rings instead. See
                        // LightEmitterConfig::toneSteps's comment.
                        float steps = static_cast<float>(cfg->toneSteps);
                        falloffT = std::floor(falloffT * steps) / steps;
                    }
                    float strength = brightness * angularFactor * std::pow(1.0f - falloffT, cfg->falloffExponent);
                    if (strength <= 0.0f) continue;

                    sprite->AccumulateLightTint(px, py, color, strength);

                    auto it = touched.find(candidate);
                    if (it == touched.end()) {
                        touched[candidate] = LitRect{candidate, px, py, px, py};
                    } else {
                        it->second.minX = std::min(it->second.minX, px);
                        it->second.minY = std::min(it->second.minY, py);
                        it->second.maxX = std::max(it->second.maxX, px);
                        it->second.maxY = std::max(it->second.maxY, py);
                    }
                }
            }
        }

        for (auto& [body, rect] : touched) newLitRects.push_back(rect);
    }

    m_PrevLitRects = std::move(newLitRects);
}