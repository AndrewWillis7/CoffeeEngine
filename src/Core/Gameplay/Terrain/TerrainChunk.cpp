#include "TerrainChunk.h"
#include "../../Math/Noise.h"
#include "../../Math/AABB.h"
#include "../../Physics/RigidBody2D.h"
#include "../../Physics/CollisionShape2D.h"
#include "../../../Renderer/PixelSprite.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

// Spring integration upper bound on dt. A frame hitch (alt-tab, a stall,
// a breakpoint) would otherwise hand the blade spring a dt large enough
// to overshoot past its equilibrium and diverge -- the same class of
// bug RigidBody2D::Integrate's drag term already documents, and the same
// fix: cap the step rather than trust the clock. Grass being a frame or
// two "behind" after a hitch is invisible; grass exploding is not.
constexpr float kMaxSpringStep = 1.0f / 30.0f;

Color Lerp(const Color& a, const Color& b, float t) {
    return Color(a.r + (b.r - a.r) * t,
                 a.g + (b.g - a.g) * t,
                 a.b + (b.b - a.b) * t,
                 a.a + (b.a - a.a) * t);
}

float SafeScale(float s) { return s != 0.0f ? s : 1.0f; }

} // namespace

// =====================================================================
// Local pixel space <-> world space.
//
// `u` here is a CONTINUOUS local pixel coordinate measured from the
// sprite's left/top EDGE, not a pixel index -- pixel index i covers
// [i, i+1), so the left edge of column i is u = i and its center is
// u = i + 0.5. Same mapping LightingSystem's WorldToPixel/PixelToWorld
// use (that pair just bakes the +0.5 in, because it always wants a
// texel's center); keeping the edge form here is what lets ResolveBody
// below talk about "the world X where column c starts" without
// off-by-half errors.
//
// Rotation is ignored on purpose -- see the header. Scale is honored so
// the collision surface stays glued to where the sprite actually draws.
// =====================================================================

float TerrainChunk::ColumnToWorldX(float u, const RigidBody2D& terrainBody) const {
    float sx = SafeScale(terrainBody.transform.scale.x);
    return terrainBody.transform.position.x + (u - terrainBody.size.x * 0.5f) * sx;
}

float TerrainChunk::WorldToColumn(float worldX, const RigidBody2D& terrainBody) const {
    float sx = SafeScale(terrainBody.transform.scale.x);
    return (worldX - terrainBody.transform.position.x) / sx + terrainBody.size.x * 0.5f;
}

// =====================================================================
// Generation
// =====================================================================

void TerrainChunk::Generate(PixelSprite& sprite) {
    m_Width = sprite.GetWidth();
    m_Height = sprite.GetHeight();
    m_SurfaceY.clear();
    m_Blades.clear();
    m_Time = 0.0f;

    if (m_Width <= 0 || m_Height <= 0) {
        std::cerr << "Engine Warning: TerrainChunk::Generate got an invalid sprite -- nothing generated.\n";
        m_Width = m_Height = 0;
        return;
    }

    grassMinHeight = std::clamp(grassMinHeight, 1, kMaxBladeHeight);
    grassMaxHeight = std::clamp(grassMaxHeight, grassMinHeight, kMaxBladeHeight);

    // The grass has to physically fit between the mean surface and the
    // sprite's top edge, worst case (a column that peaked a full
    // amplitude high, carrying the tallest possible blade). Clamping here
    // rather than silently generating clipped grass -- an easy mistake to
    // make from Lua, and a confusing one to debug from the picture alone,
    // since the dirt would look completely correct.
    float minOffset = surfaceAmplitude + static_cast<float>(grassMaxHeight) + 1.0f;
    if (surfaceOffset < minOffset) {
        std::cerr << "Engine Warning: TerrainChunk surfaceOffset (" << surfaceOffset
                  << ") leaves no room for grass -- raising it to " << minOffset
                  << ". Give the chunk a taller sprite or a smaller amplitude.\n";
        surfaceOffset = minOffset;
    }

    m_SurfaceY.resize(static_cast<size_t>(m_Width));

    // ---- Pass 1: the heightmap ----------------------------------------
    for (int x = 0; x < m_Width; ++x) {
        float n = Noise::FBM1DSigned(static_cast<float>(x) * surfaceFrequency,
                                     surfaceOctaves, surfaceLacunarity, surfaceGain, seed);
        float raw = surfaceOffset + n * surfaceAmplitude;

        // Snapped to a whole texel, NOT kept fractional. The dirt can only
        // start on a pixel boundary anyway, so letting collision believe
        // in a surface half a texel away from the one you can see is a
        // guaranteed "the player sinks slightly into the ground" bug.
        // One number, used by the fill, the grass roots and ResolveBody
        // alike -- so all three agree exactly.
        m_SurfaceY[static_cast<size_t>(x)] =
            std::floor(std::clamp(raw, 1.0f, static_cast<float>(m_Height - 1)));
    }

    // ---- Pass 2: paint every pixel ------------------------------------
    for (int x = 0; x < m_Width; ++x) {
        int top = static_cast<int>(m_SurfaceY[static_cast<size_t>(x)]);

        for (int y = 0; y < m_Height; ++y) {
            if (y < top) {
                // Above the surface: explicitly cleared rather than left
                // alone, so a second Generate() call with different config
                // can't leave last generation's dirt floating in the sky.
                sprite.SetPixel(x, y, Color::Transparent());
                continue;
            }

            float mottle = Noise::FBM2D01(static_cast<float>(x) * dirtFrequency,
                                          static_cast<float>(y) * dirtFrequency,
                                          dirtOctaves, 2.0f, 0.5f, seed + 7717);
            mottle = Noise::Quantize01(mottle, dirtToneSteps);
            Color c = Lerp(dirtDark, dirtLight, mottle);

            // Depth gradient, in texels below THIS column's own surface
            // rather than below the sprite's top edge -- otherwise a
            // column sitting in a dip reads as already half-buried and
            // the shading follows the sprite's rectangle instead of the
            // terrain's actual shape.
            int depthBelow = y - top;
            int columnDepth = std::max(1, m_Height - top);
            float depth01 = std::clamp(static_cast<float>(depthBelow) / static_cast<float>(columnDepth), 0.0f, 1.0f);
            float shade = 1.0f - depth01 * depthDarkening;
            c = Color(c.r * shade, c.g * shade, c.b * shade, 1.0f);

            if (topsoilDepth > 0 && depthBelow < topsoilDepth) {
                // Blend rather than replace, so the band picks up the same
                // mottling as the dirt under it and doesn't read as a
                // painted-on stripe.
                float t = 1.0f - static_cast<float>(depthBelow) / static_cast<float>(topsoilDepth);
                c = Lerp(c, Color(topsoilColor.r * shade, topsoilColor.g * shade, topsoilColor.b * shade, 1.0f), t * 0.85f);
            }

            if (Noise::Hash01(x, y, seed + 4441) < rockChance) {
                c = Lerp(Color(rockColor.r * shade, rockColor.g * shade, rockColor.b * shade, 1.0f), c, 0.25f);
            }

            sprite.SetPixel(x, y, c);
        }
    }

    // ---- Pass 3: seed the grass ---------------------------------------
    for (int x = 0; x < m_Width; ++x) {
        if (Noise::Hash01(x, 3, seed + 991) > grassDensity) continue;

        int top = static_cast<int>(m_SurfaceY[static_cast<size_t>(x)]);
        int rootY = top - 1; // first air pixel above the dirt
        if (rootY < 0) continue;

        GrassBlade blade;
        blade.column = x;
        blade.rootY = rootY;

        int span = grassMaxHeight - grassMinHeight + 1;
        blade.height = grassMinHeight + static_cast<int>(Noise::Hash01(x, 5, seed + 313) * static_cast<float>(span));
        blade.height = std::clamp(blade.height, grassMinHeight, grassMaxHeight);
        if (blade.rootY - blade.height + 1 < 0) blade.height = blade.rootY + 1;
        if (blade.height <= 0) continue;

        // Per-blade phase jitter ON TOP OF the positional wave term in
        // Update() -- the positional term alone gives a perfectly regular
        // wave train (every blade at the same x-offset leaning identically
        // forever), which reads as a flag rather than a field.
        blade.phase = Noise::Hash01(x, 9, seed + 577) * 6.28318f;
        blade.tint = Noise::Hash01(x, 13, seed + 733);

        m_Blades.push_back(blade);
    }
}

// =====================================================================
// Per-frame grass
// =====================================================================

void TerrainChunk::Update(PixelSprite& sprite, const RigidBody2D& terrainBody,
                          const std::vector<const RigidBody2D*>& disturbers, float deltaTime) {
    if (!IsGenerated() || m_Blades.empty()) return;

    m_Time += deltaTime;
    float step = std::min(deltaTime, kMaxSpringStep);

    for (const RigidBody2D* disturber : disturbers) {
        if (disturber) ApplyDisturber(*disturber, terrainBody, step);
    }

    for (GrassBlade& blade : m_Blades) {
        // Travelling wave: the phase advances with the blade's own column,
        // so the gust visibly moves along the ground left-to-right instead
        // of the whole field pulsing at once. Two non-harmonic terms (the
        // same trick LightingSystem's FlickerFactor uses) keep it from
        // reading as a metronome.
        float wave = std::sin(m_Time * swaySpeed
                              + static_cast<float>(blade.column) * swayPhasePerTexel
                              + blade.phase);
        float detail = std::sin(m_Time * swaySpeed * 1.7f
                                + static_cast<float>(blade.column) * swayPhasePerTexel * 0.6f
                                + blade.phase * 1.3f);
        float target = swayAmplitude * (0.78f * wave + 0.22f * detail);

        // Damped spring toward that target. The disturbers above have
        // already injected velocity into bendVel this frame, so a shove
        // and the idle sway share one integrator rather than fighting
        // over the blade's position -- which is what makes a blade
        // knocked aside settle back INTO the wind rather than snapping to
        // upright and then starting to sway again.
        float accel = (target - blade.bend) * bendStiffness - blade.bendVel * bendDamping;
        blade.bendVel += accel * step;
        blade.bend += blade.bendVel * step;

        if (blade.bend > maxBend) { blade.bend = maxBend; blade.bendVel = std::min(blade.bendVel, 0.0f); }
        if (blade.bend < -maxBend) { blade.bend = -maxBend; blade.bendVel = std::max(blade.bendVel, 0.0f); }
    }

    EraseBlades(sprite);
    DrawBlades(sprite);
}

void TerrainChunk::ApplyDisturber(const RigidBody2D& disturber, const RigidBody2D& terrainBody, float deltaTime) {
    AABB box = disturber.collisionShape
        ? disturber.collisionShape->GetWorldAABB(disturber.transform)
        : AABB{disturber.transform.position, disturber.size * 0.5f};

    Vector2 boxMin = box.Min();
    Vector2 boxMax = box.Max();

    float uMin = WorldToColumn(boxMin.x, terrainBody) - disturbPadding;
    float uMax = WorldToColumn(boxMax.x, terrainBody) + disturbPadding;
    if (uMax < 0.0f || uMin > static_cast<float>(m_Width)) return;

    float sy = SafeScale(terrainBody.transform.scale.y);
    float localTopWorldY = terrainBody.transform.position.y - terrainBody.size.y * 0.5f * sy;

    float speed = std::abs(disturber.velocity.x);
    float bodyCenterX = box.center.x;

    for (GrassBlade& blade : m_Blades) {
        float col = static_cast<float>(blade.column) + 0.5f;
        if (col < uMin || col > uMax) continue;

        // Vertical gate -- a character jumping clean over the grass, or
        // standing on a ledge above it, shouldn't be parting it. Compared
        // against the blade's own tip/root, not the chunk's rectangle,
        // which matters on uneven ground where a blade in a dip sits well
        // below one on the next rise.
        float tipWorldY = localTopWorldY + static_cast<float>(blade.rootY - blade.height + 1) * sy;
        float rootWorldY = localTopWorldY + static_cast<float>(blade.rootY + 1) * sy;
        float padY = disturbPadding * sy;
        if (boxMax.y < tipWorldY - padY || boxMin.y > rootWorldY + padY) continue;

        // 1 while the blade is inside the body's own x-span, tapering to 0
        // across the padding either side -- so grass at the very edge of
        // a footstep only twitches.
        float overshoot = 0.0f;
        if (col < WorldToColumn(boxMin.x, terrainBody)) overshoot = WorldToColumn(boxMin.x, terrainBody) - col;
        else if (col > WorldToColumn(boxMax.x, terrainBody)) overshoot = col - WorldToColumn(boxMax.x, terrainBody);
        float falloff = disturbPadding > 0.0f ? std::clamp(1.0f - overshoot / disturbPadding, 0.0f, 1.0f) : 1.0f;
        if (falloff <= 0.0f) continue;

        // Moving: sweep the grass the way you're going. Standing still:
        // splay it away from you, so a character at rest sits in a small
        // parted patch instead of a field that quietly snaps upright the
        // instant they stop walking.
        float dir;
        if (speed > 5.0f) dir = disturber.velocity.x > 0.0f ? 1.0f : -1.0f;
        else {
            float bladeWorldX = ColumnToWorldX(col, terrainBody);
            dir = (bladeWorldX >= bodyCenterX) ? 1.0f : -1.0f;
        }

        float push = disturbStrength * falloff * (1.0f + speed * disturbSpeedScale);
        blade.bendVel += dir * push * deltaTime;
    }
}

void TerrainChunk::EraseBlades(PixelSprite& sprite) {
    for (GrassBlade& blade : m_Blades) {
        if (!blade.drawn) continue;

        for (int seg = 0; seg < blade.height; ++seg) {
            int px = blade.column + blade.lastOffset[seg];
            int py = blade.rootY - seg;
            if (px < 0 || px >= m_Width || py < 0) continue;

            // The guard that makes erasing exact instead of destructive:
            // a blade leaning over a NEIGHBORING column can end up above a
            // pixel that is that column's dirt (or that column's own
            // grass). Only ever clear a pixel that's air in the column it
            // actually landed in.
            if (static_cast<float>(py) >= m_SurfaceY[static_cast<size_t>(px)]) continue;

            sprite.SetPixel(px, py, Color::Transparent());
        }
        blade.drawn = false;
    }
}

void TerrainChunk::DrawBlades(PixelSprite& sprite) {
    for (GrassBlade& blade : m_Blades) {
        float height = static_cast<float>(blade.height);
        int prevOffset = 0;

        for (int seg = 0; seg < blade.height; ++seg) {
            // t: 0 at the root, 1 at the tip. The offset is t^2, not t --
            // a real blade pivots at its base, so the bottom pixel should
            // barely move while the tip carries almost all of the
            // displacement. Linear looks like the whole blade sliding
            // sideways.
            float t = static_cast<float>(seg + 1) / height;
            float offset = blade.bend * t * t;

            int dx = static_cast<int>(std::lround(offset));

            // Never let one segment jump more than a single column past
            // the one below it. A t^2 curve on a hard-bent blade can round
            // two adjacent segments to offsets 2 apart, and at one texel
            // wide that leaves a diagonal GAP -- the blade visually breaks
            // into a stub and a floating green pixel. Clamping to +/-1 per
            // segment keeps every blade 8-connected root to tip, which is
            // what makes it read as one object bending rather than a
            // column of separate dots. Costs a little bend range on the
            // tallest blades, which is invisible; the alternative isn't.
            dx = std::clamp(dx, prevOffset - 1, prevOffset + 1);
            dx = std::clamp(dx, -kMaxBladeHeight, kMaxBladeHeight);
            prevOffset = dx;
            blade.lastOffset[seg] = static_cast<int8_t>(dx);

            int px = blade.column + dx;
            int py = blade.rootY - seg;
            if (px < 0 || px >= m_Width || py < 0) continue;
            if (static_cast<float>(py) >= m_SurfaceY[static_cast<size_t>(px)]) continue;

            // Tips lighter than roots -- the cheapest way to make 3-5
            // pixels read as a blade with a direction rather than a green
            // tally mark. Per-blade tint on top of that keeps the field
            // from looking like one repeated stamp.
            float mix = std::clamp(blade.tint * 0.55f + t * 0.5f, 0.0f, 1.0f);
            sprite.SetPixel(px, py, Lerp(grassDark, grassLight, mix));
        }
        blade.drawn = true;
    }
}

// =====================================================================
// Queries and collision
// =====================================================================

float TerrainChunk::SurfaceWorldY(float worldX, const RigidBody2D& terrainBody) const {
    float sy = SafeScale(terrainBody.transform.scale.y);
    float localTopWorldY = terrainBody.transform.position.y - terrainBody.size.y * 0.5f * sy;
    if (!IsGenerated()) return localTopWorldY + terrainBody.size.y * sy;

    float u = WorldToColumn(worldX, terrainBody);
    int c = std::clamp(static_cast<int>(std::floor(u)), 0, m_Width - 1);
    return localTopWorldY + m_SurfaceY[static_cast<size_t>(c)] * sy;
}

bool TerrainChunk::ResolveBody(RigidBody2D& body, const RigidBody2D& terrainBody) const {
    if (!IsGenerated() || &body == &terrainBody) return false;

    float sy = SafeScale(terrainBody.transform.scale.y);
    float localTopWorldY = terrainBody.transform.position.y - terrainBody.size.y * 0.5f * sy;

    bool resolved = false;

    // At most two passes. The first can shove the body sideways out of a
    // rise it isn't allowed to climb; the second then lands it on
    // whatever it's standing over once it's been moved. Bounded rather
    // than looped-to-convergence on purpose -- one horizontal and one
    // vertical correction is all a single frame's movement can actually
    // need, and a convergence loop against a noisy heightfield is exactly
    // the kind of thing that finds a corner it can oscillate in forever.
    for (int pass = 0; pass < 2; ++pass) {
        AABB box = body.collisionShape
            ? body.collisionShape->GetWorldAABB(body.transform)
            : AABB{body.transform.position, body.size * 0.5f};

        Vector2 boxMin = box.Min();
        Vector2 boxMax = box.Max();

        float uMin = WorldToColumn(boxMin.x, terrainBody);
        float uMax = WorldToColumn(boxMax.x, terrainBody);
        if (uMax <= 0.0f || uMin >= static_cast<float>(m_Width)) return resolved; // beside the chunk entirely

        int c0 = std::clamp(static_cast<int>(std::floor(uMin)), 0, m_Width - 1);
        int c1 = std::clamp(static_cast<int>(std::ceil(uMax)) - 1, 0, m_Width - 1);
        if (c1 < c0) c1 = c0;

        // Highest surface (SMALLEST world Y -- +y is down) anywhere under
        // the body, not the surface under its center. A box resting on a
        // bumpy heightfield sits on the tallest thing beneath it; sampling
        // the center instead lets a bump under one corner get swallowed
        // and the body visibly clips through it.
        float highest = localTopWorldY + m_SurfaceY[static_cast<size_t>(c0)] * sy;
        for (int c = c0 + 1; c <= c1; ++c) {
            highest = std::min(highest, localTopWorldY + m_SurfaceY[static_cast<size_t>(c)] * sy);
        }

        float penetration = boxMax.y - highest;
        if (penetration <= 0.0f) return resolved; // airborne / clear of the ground

        // Fully below the surface: deliberately NOT resolved. There's no
        // "correct" way out of solid ground for a heightfield -- pushing
        // up would teleport a body that fell through a chunk's open end
        // and wandered back underneath it straight to the top of the
        // world. Bodies that belong under the terrain (a future cave, a
        // buried secret) get to stay there.
        if (boxMin.y >= highest) return resolved;

        if (penetration <= maxStepHeight) {
            body.ApplyCollisionCorrection(Vector2(0.0f, -penetration));
            return true; // vertical resolution is terminal -- we're standing on it
        }

        if (pass == 1) return resolved; // already shoved once this call; don't ping-pong

        // Too tall to step onto: treat the offending columns as a wall and
        // resolve along X instead. Which way out depends on which way the
        // body was travelling -- and when it isn't travelling at all
        // (spawned in a cliff, pushed in by something else), take the
        // shorter way out.
        float stepLimitY = boxMax.y - maxStepHeight;
        int firstBlocking = -1;
        int lastBlocking = -1;
        for (int c = c0; c <= c1; ++c) {
            if (localTopWorldY + m_SurfaceY[static_cast<size_t>(c)] * sy < stepLimitY) {
                if (firstBlocking < 0) firstBlocking = c;
                lastBlocking = c;
            }
        }
        if (firstBlocking < 0) return resolved; // can't happen (penetration > step implies one), but don't trust it

        float pushLeft = ColumnToWorldX(static_cast<float>(firstBlocking), terrainBody) - boxMax.x;    // negative
        float pushRight = ColumnToWorldX(static_cast<float>(lastBlocking + 1), terrainBody) - boxMin.x; // positive

        float correctionX;
        if (body.velocity.x > 1.0f)        correctionX = pushLeft;
        else if (body.velocity.x < -1.0f)  correctionX = pushRight;
        else correctionX = (std::abs(pushLeft) <= std::abs(pushRight)) ? pushLeft : pushRight;

        body.ApplyCollisionCorrection(Vector2(correctionX, 0.0f));
        resolved = true;
        // ...and round again, so we still land on whatever is under us now.
    }

    return resolved;
}