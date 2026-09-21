#include "LightingSystem.h"
#include "LightEmitterConfig.h"
#include "../ActorRegistry.h"
#include "../Physics/RigidBody2D.h"
#include "../Math/AABB.h"
#include "../../Renderer/PixelSprite.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadToDeg = 180.0f / kPi;

// The only thing left that steps through world space piece by piece. It just has
// to be fine enough that a thin wall cannot be stepped clean over; it no longer
// has anything to do with how gapless the lighting itself looks.
constexpr float kOcclusionStepPixels = 1.0f;

// Conservative world AABB for a sprite-backed body's VISUAL extent -- not
// CollisionShape2D::GetWorldAABB, which can be smaller, offset, or absent (an
// art object has no collider but still catches light). Padded to the diagonal so
// a rotated sprite's footprint is never underestimated.
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

// Two non-harmonic sines, so the flicker reads as organic rather than a single
// sine's metronome. `seed` keeps multiple torches out of lockstep.
float FlickerFactor(float time, float speed, float seed) {
    float t = time * speed;
    float wobble = std::sin(t + seed * 6.2831853f) * 0.6f
                 + std::sin(t * 2.37f + seed * 11.0f) * 0.4f;
    return std::clamp(wobble, -1.0f, 1.0f);
}

// Signed degree difference, shortest way around the circle, in (-180, 180].
float AngleDiffDeg(float angleDeg, float centerDeg) {
    float diff = angleDeg - centerDeg;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff <= -180.0f) diff += 360.0f;
    return diff;
}

} // namespace

// True if some blocker's solid pixel sits strictly between `from` (a light) and
// `to` (a candidate pixel). `skipBody` is the light's OWN body, excluded so
// something that both emits and blocks never shadows its own glow.
//
// Deliberately not the target pixel's body: a solid wall SHOULD shadow its own
// far side from a light on the near side -- that is the point of lightBlocking.
// Only the destination texel itself is protected, by stopping a step short.
//
// Per blocker the ray is slab-clipped against that blocker's world AABB before
// any stepping, and only the clipped-in overlap is marched. A ray with clear
// line of sight -- overwhelmingly the common case -- costs one slab test rather
// than distance/kOcclusionStepPixels sample lookups.
bool LightingSystem::IsOccluded(const Vector2& from, const Vector2& to, const RigidBody2D* skipBody,
                                const std::vector<Blocker>& blockers) {
    if (blockers.empty()) return false;

    Vector2 delta = to - from;
    float dist = delta.Length();
    if (dist <= kOcclusionStepPixels) return false; // essentially at the light itself

    Vector2 dir = delta / dist;
    float marchLimit = dist - kOcclusionStepPixels;

    for (const Blocker& blocker : blockers) {
        if (blocker.body == skipBody) continue;

        // Standard ray-vs-AABB clip, once per blocker per ray rather than once
        // per sample per blocker per ray.
        float tEnter = kOcclusionStepPixels, tExit = marchLimit;

        for (int axis = 0; axis < 2; ++axis) {
            float origin = axis == 0 ? from.x : from.y;
            float d      = axis == 0 ? dir.x  : dir.y;
            float lo     = axis == 0 ? blocker.boxMin.x : blocker.boxMin.y;
            float hi     = axis == 0 ? blocker.boxMax.x : blocker.boxMax.y;

            if (std::abs(d) < 1e-6f) {
                // Parallel to this slab: either already inside it (this axis
                // doesn't narrow anything) or it can never enter the box.
                if (origin < lo || origin > hi) { tEnter = 1.0f; tExit = 0.0f; break; }
                continue;
            }
            float ta = (lo - origin) / d;
            float tb = (hi - origin) / d;
            if (ta > tb) std::swap(ta, tb);
            tEnter = std::max(tEnter, ta);
            tExit  = std::min(tExit,  tb);
            if (tEnter > tExit) break;
        }
        if (tEnter > tExit) continue; // ray never enters this blocker's box

        for (float traveled = tEnter; traveled < tExit; traveled += kOcclusionStepPixels) {
            float wx = from.x + dir.x * traveled;
            float wy = from.y + dir.y * traveled;

            // Inverse of the vertex shader's local->world quad transform, using
            // the transform cached on the blocker.
            float dx = wx - blocker.pos.x;
            float dy = wy - blocker.pos.y;
            float lx = dx, ly = dy;
            if (blocker.rotated) {
                lx = dx * blocker.cosNeg - dy * blocker.sinNeg;
                ly = dx * blocker.sinNeg + dy * blocker.cosNeg;
            }
            int px = static_cast<int>(std::floor(lx * blocker.invScaleX + blocker.halfW));
            int py = static_cast<int>(std::floor(ly * blocker.invScaleY + blocker.halfH));
            if (px < 0 || py < 0 || px >= blocker.width || py >= blocker.height) continue;
            if (blocker.sprite->IsSolid(px, py)) return true;
        }
    }
    return false;
}

void LightingSystem::Update(ActorRegistry& actors, float deltaTime) {
    const auto profileStart = std::chrono::steady_clock::now();
    m_Stats = Stats{};

    m_Time += deltaTime;

    const auto& bodies = actors.GetBodies();

    // Pass 0: find every active light.
    m_Lights.clear();
    for (const auto& bodyPtr : bodies) {
        RigidBody2D* body = bodyPtr.get();
        if (body->lightEmitter && !body->destroyed) m_Lights.push_back({body, body->lightEmitter});
    }

    // Pass 1: erase what LAST frame's lights touched, before this frame
    // re-accumulates, so a light that moved leaves no stale patch behind.
    for (const LitRect& rect : m_PrevLitRects) {
        if (rect.body->sprite) rect.body->sprite->ResetLightingRect(rect.minX, rect.minY, rect.maxX, rect.maxY);
    }

    m_Stats.lights = static_cast<int>(m_Lights.size());

    if (m_Lights.empty()) {
        m_PrevLitRects.clear();
        m_Stats.milliseconds = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - profileStart).count();
        return;
    }

    // Which bodies cast shadows doesn't depend on which light is asking, so this
    // is gathered once per frame, transforms and all.
    m_Blockers.clear();
    for (const auto& bodyPtr : bodies) {
        RigidBody2D* body = bodyPtr.get();
        if (!body->sprite || !body->lightBlocking) continue;

        AABB box = SpriteWorldAABB(*body);
        const float rot = body->transform.rotation;
        const float sx = body->transform.scale.x != 0.0f ? body->transform.scale.x : 1.0f;
        const float sy = body->transform.scale.y != 0.0f ? body->transform.scale.y : 1.0f;

        Blocker b;
        b.body = body;
        b.sprite = body->sprite;
        b.boxMin = box.Min();
        b.boxMax = box.Max();
        b.pos = body->transform.position;
        b.rotated = (rot != 0.0f);
        b.cosNeg = b.rotated ? std::cos(-rot) : 1.0f;
        b.sinNeg = b.rotated ? std::sin(-rot) : 0.0f;
        b.invScaleX = 1.0f / sx;
        b.invScaleY = 1.0f / sy;
        b.halfW = body->size.x * 0.5f;
        b.halfH = body->size.y * 0.5f;
        b.width = body->sprite->GetWidth();
        b.height = body->sprite->GetHeight();
        m_Blockers.push_back(b);
    }

    m_LitRectScratch.clear();

    // Pass 2: light every candidate pixel of every candidate body, per light.
    for (const LightSample& light : m_Lights) {
        RigidBody2D* lightBody = light.body;
        LightEmitterConfig* cfg = light.config;
        if (cfg->radius <= 0.0f) continue;

        // Sampled once per light per frame, never per pixel, so a frame's lit
        // region never seams from its own flicker.
        float seed = static_cast<float>(reinterpret_cast<uintptr_t>(cfg) % 10007) / 10007.0f;
        float flicker = cfg->flicker ? FlickerFactor(m_Time, cfg->flickerSpeed, seed) : 0.0f;
        float brightness = std::max(0.0f, cfg->brightness * (1.0f + flicker * cfg->flickerIntensityAmount));
        if (brightness <= 0.0f) continue;

        Color color = cfg->color;
        if (cfg->flicker) {
            float shift = std::max(0.0f, flicker); // only warms on the upswing
            color.r += cfg->flickerColorShift.r * shift;
            color.g += cfg->flickerColorShift.g * shift;
            color.b += cfg->flickerColorShift.b * shift;
        }

        const Vector2 lightPos = lightBody->transform.position;
        const float radius = cfg->radius;
        const float radiusSq = radius * radius;
        const float invRadius = 1.0f / radius;

        // Broad-phase: which sprite-backed bodies are worth per-pixel testing.
        m_Candidates.clear();
        for (const auto& bodyPtr : bodies) {
            RigidBody2D* candidate = bodyPtr.get();
            if (!candidate->sprite) continue;
            AABB box = SpriteWorldAABB(*candidate);
            float reach = radius + box.halfExtents.x; // square box, either axis works
            if (Vector2::Distance(lightPos, box.center) > reach) continue;
            m_Candidates.push_back(candidate);
        }
        if (m_Candidates.empty()) continue;

        bool isCone = cfg->type == LightEmitterConfig::Type::Cone;
        float dirDeg = 0.0f, sweepDeg = 360.0f;
        if (isCone) {
            dirDeg = cfg->coneDirectionRad * kRadToDeg;
            if (cfg->useOwnerRotation) dirDeg += lightBody->transform.rotation * kRadToDeg;
            sweepDeg = std::max(0.0f, cfg->coneAngleRad * kRadToDeg);
        }
        if (sweepDeg <= 0.0f) continue;
        const float halfAngleDeg = sweepDeg * 0.5f;
        const float coneFadeStart = halfAngleDeg * 0.8f;

        // std::pow dominates the per-lit-pixel cost at the two exponents anyone
        // actually uses, so special-case them.
        const float falloffExp = cfg->falloffExponent;
        const int falloffMode = (falloffExp == 2.0f) ? 2 : (falloffExp == 1.0f) ? 1 : 0;

        const int toneSteps = cfg->toneSteps;
        const float toneStepsF = static_cast<float>(toneSteps);
        const float invToneSteps = toneSteps > 0 ? 1.0f / toneStepsF : 0.0f;

        for (RigidBody2D* candidate : m_Candidates) {
            PixelSprite* sprite = candidate->sprite;
            const int w = sprite->GetWidth();
            const int h = sprite->GetHeight();

            // Local->world transform, hoisted out of the pixel loops.
            const Vector2 cpos = candidate->transform.position;
            const float crot = candidate->transform.rotation;
            const bool rotated = (crot != 0.0f);
            const float cosR = rotated ? std::cos(crot) : 1.0f;
            const float sinR = rotated ? std::sin(crot) : 0.0f;
            const float csx = candidate->transform.scale.x != 0.0f ? candidate->transform.scale.x : 1.0f;
            const float csy = candidate->transform.scale.y != 0.0f ? candidate->transform.scale.y : 1.0f;
            const float chalfW = candidate->size.x * 0.5f;
            const float chalfH = candidate->size.y * 0.5f;

            // Clamp the pixel walk to the rect this light's circle can actually
            // reach on THIS candidate, by pushing the light's world AABB corners
            // through the inverse transform. Affine, so the bound stays
            // conservative under rotation and scale -- never too small. A light
            // in the corner of a large floor no longer walks every pixel of it.
            int rminX, rminY, rmaxX, rmaxY;
            {
                const float cornerX[4] = {lightPos.x - radius, lightPos.x + radius,
                                          lightPos.x - radius, lightPos.x + radius};
                const float cornerY[4] = {lightPos.y - radius, lightPos.y - radius,
                                          lightPos.y + radius, lightPos.y + radius};
                float loX = 1e30f, hiX = -1e30f, loY = 1e30f, hiY = -1e30f;

                for (int i = 0; i < 4; ++i) {
                    Vector2 d(cornerX[i] - cpos.x, cornerY[i] - cpos.y);
                    Vector2 local = rotated ? d.Rotated(-crot) : d;
                    float pxf = local.x / csx + chalfW;
                    float pyf = local.y / csy + chalfH;
                    loX = std::min(loX, pxf); hiX = std::max(hiX, pxf);
                    loY = std::min(loY, pyf); hiY = std::max(hiY, pyf);
                }
                rminX = std::max(0, static_cast<int>(std::floor(loX)));
                rminY = std::max(0, static_cast<int>(std::floor(loY)));
                rmaxX = std::min(w - 1, static_cast<int>(std::ceil(hiX)));
                rmaxY = std::min(h - 1, static_cast<int>(std::ceil(hiY)));
            }

            // This light's touched rect on this candidate, accumulated locally
            // and pushed once -- each candidate is visited exactly once here.
            bool touched = false;
            int tMinX = 0, tMinY = 0, tMaxX = 0, tMaxY = 0;

            for (int py = rminY; py <= rmaxY; ++py) {
                const float ly = ((static_cast<float>(py) + 0.5f) - chalfH) * csy;

                for (int px = rminX; px <= rmaxX; ++px) {
                    if (!sprite->IsSolid(px, py)) continue;

                    const float lx = ((static_cast<float>(px) + 0.5f) - chalfW) * csx;
                    const Vector2 worldPos = rotated
                        ? Vector2(cpos.x + lx * cosR - ly * sinR, cpos.y + lx * sinR + ly * cosR)
                        : Vector2(cpos.x + lx, cpos.y + ly);

                    // Squared first -- the sqrt is only paid by pixels that are
                    // actually in range, not by the whole bounding rect.
                    const float toX = worldPos.x - lightPos.x;
                    const float toY = worldPos.y - lightPos.y;
                    const float distSq = toX * toX + toY * toY;
                    if (distSq > radiusSq) continue;

                    // Soft cone edge: full strength across the inner 80% of the
                    // arc, fading to zero at the boundary. Always 1 for a Point.
                    float angularFactor = 1.0f;
                    if (isCone) {
                        if (distSq > 0.0001f) {
                            float angleDeg = std::atan2(toY, toX) * kRadToDeg;
                            float offCenterDeg = std::abs(AngleDiffDeg(angleDeg, dirDeg));
                            if (offCenterDeg > halfAngleDeg) continue; // outside the wedge
                            angularFactor = 1.0f - Smoothstep01(coneFadeStart, halfAngleDeg, offCenterDeg);
                            if (angularFactor <= 0.0f) continue;
                        }
                        // else: pixel sits on the light itself, direction is
                        // undefined -- treat it as dead-center.
                    }

                    // Only now, after the cheap rejections: this is the expensive part.
                    if (IsOccluded(lightPos, worldPos, lightBody, m_Blockers)) continue;

                    const float dist = std::sqrt(distSq);
                    float falloffT = dist * invRadius;
                    if (falloffT > 1.0f) falloffT = 1.0f;
                    if (toneSteps > 0) {
                        // Round DOWN to the nearest 1/toneSteps, turning the
                        // gradient into flat concentric rings.
                        falloffT = std::floor(falloffT * toneStepsF) * invToneSteps;
                    }

                    const float base = 1.0f - falloffT;
                    const float falloff = (falloffMode == 2) ? base * base
                                        : (falloffMode == 1) ? base
                                        : std::pow(base, falloffExp);
                    const float strength = brightness * angularFactor * falloff;
                    if (strength <= 0.0f) continue;

                    sprite->AccumulateLightTint(px, py, color, strength);
                    ++m_Stats.litPixels;

                    if (!touched) {
                        touched = true;
                        tMinX = tMaxX = px;
                        tMinY = tMaxY = py;
                    } else {
                        if (px < tMinX) tMinX = px;
                        if (px > tMaxX) tMaxX = px;
                        if (py < tMinY) tMinY = py;
                        if (py > tMaxY) tMaxY = py;
                    }
                }
            }

            if (touched) m_LitRectScratch.push_back(LitRect{candidate, tMinX, tMinY, tMaxX, tMaxY});
        }
    }

    // Swap rather than assign, so both vectors keep their capacity for next frame.
    m_PrevLitRects.swap(m_LitRectScratch);

    m_Stats.blockers = static_cast<int>(m_Blockers.size());
    m_Stats.milliseconds = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - profileStart).count();
}
