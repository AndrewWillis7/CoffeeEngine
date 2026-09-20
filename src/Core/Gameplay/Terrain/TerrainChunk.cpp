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

// Upper bound on the spring's dt. A frame hitch would otherwise hand the blades
// a step large enough to overshoot equilibrium and diverge -- the same class of
// bug RigidBody2D::Integrate's drag term guards against. Grass a frame behind
// after a hitch is invisible; grass exploding is not.
constexpr float kMaxSpringStep = 1.0f / 30.0f;

Color Lerp(const Color& a, const Color& b, float t) {
    return Color(a.r + (b.r - a.r) * t,
                 a.g + (b.g - a.g) * t,
                 a.b + (b.b - a.b) * t,
                 a.a + (b.a - a.a) * t);
}

float SafeScale(float s) { return s != 0.0f ? s : 1.0f; }

} // namespace

// `u` is a CONTINUOUS local pixel coordinate from the sprite's left edge, not an
// index: column i covers [i, i+1). Keeping the edge form (rather than baking in
// the +0.5 the way LightingSystem does) is what lets ResolveBody talk about
// "the world X where column c starts" without off-by-half errors.

float TerrainChunk::ColumnToWorldX(float u, const RigidBody2D& terrainBody) const {
    float sx = SafeScale(terrainBody.transform.scale.x);
    return terrainBody.transform.position.x + (u - terrainBody.size.x * 0.5f) * sx;
}

float TerrainChunk::WorldToColumn(float worldX, const RigidBody2D& terrainBody) const {
    float sx = SafeScale(terrainBody.transform.scale.x);
    return (worldX - terrainBody.transform.position.x) / sx + terrainBody.size.x * 0.5f;
}

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

    // Worst case: a column peaked a full amplitude high carrying the tallest
    // possible blade. Clamped rather than silently clipped, since the dirt would
    // still look correct and the mistake is easy to make from Lua.
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

        // Snapped to a whole texel: dirt can only start on a pixel boundary, so a
        // fractional surface would mean the player sinks slightly into visible
        // ground. One number shared by the fill, the roots and ResolveBody.
        m_SurfaceY[static_cast<size_t>(x)] =
            std::floor(std::clamp(raw, 1.0f, static_cast<float>(m_Height - 1)));
    }

    // ---- Pass 2: paint every pixel ------------------------------------
    for (int x = 0; x < m_Width; ++x) {
        int top = static_cast<int>(m_SurfaceY[static_cast<size_t>(x)]);

        for (int y = 0; y < m_Height; ++y) {
            if (y < top) {
                // Cleared explicitly, so a second Generate() can't leave the last
                // generation's dirt floating in the sky.
                sprite.SetPixel(x, y, Color::Transparent());
                continue;
            }

            float mottle = Noise::FBM2D01(static_cast<float>(x) * dirtFrequency,
                                          static_cast<float>(y) * dirtFrequency,
                                          dirtOctaves, 2.0f, 0.5f, seed + 7717);
            mottle = Noise::Quantize01(mottle, dirtToneSteps);
            Color c = Lerp(dirtDark, dirtLight, mottle);

            // Measured below THIS column's surface, not the sprite's top edge --
            // otherwise a column in a dip reads as half-buried and the shading
            // follows the sprite's rectangle instead of the terrain's shape.
            int depthBelow = y - top;
            int columnDepth = std::max(1, m_Height - top);
            float depth01 = std::clamp(static_cast<float>(depthBelow) / static_cast<float>(columnDepth), 0.0f, 1.0f);
            float shade = 1.0f - depth01 * depthDarkening;
            c = Color(c.r * shade, c.g * shade, c.b * shade, 1.0f);

            if (topsoilDepth > 0 && depthBelow < topsoilDepth) {
                // Blended, not replaced, so the band picks up the same mottling as
                // the dirt under it instead of reading as a painted-on stripe.
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

        // Jitter on top of Update()'s positional wave: that term alone gives a
        // perfectly regular wave train, which reads as a flag, not a field.
        blade.phase = Noise::Hash01(x, 9, seed + 577) * 6.28318f;
        blade.tint = Noise::Hash01(x, 13, seed + 733);

        m_Blades.push_back(blade);
    }
}

void TerrainChunk::Update(PixelSprite& sprite, const RigidBody2D& terrainBody,
                          const std::vector<const RigidBody2D*>& disturbers, float deltaTime) {
    if (!IsGenerated() || m_Blades.empty()) return;

    m_Time += deltaTime;
    float step = std::min(deltaTime, kMaxSpringStep);

    for (const RigidBody2D* disturber : disturbers) {
        if (disturber) ApplyDisturber(*disturber, terrainBody, step);
    }

    for (GrassBlade& blade : m_Blades) {
        // Travelling wave: phase advances with the blade's column, so the gust
        // moves along the ground instead of the field pulsing at once. Two
        // non-harmonic terms keep it from reading as a metronome.
        float wave = std::sin(m_Time * swaySpeed
                              + static_cast<float>(blade.column) * swayPhasePerTexel
                              + blade.phase);
        float detail = std::sin(m_Time * swaySpeed * 1.7f
                                + static_cast<float>(blade.column) * swayPhasePerTexel * 0.6f
                                + blade.phase * 1.3f);
        float target = swayAmplitude * (0.78f * wave + 0.22f * detail);

        // The disturbers above already injected into bendVel, so a shove and the
        // idle sway share one integrator instead of fighting over the blade's
        // position -- which is what lets a knocked blade settle back INTO the
        // wind rather than snapping upright and starting over.
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

    // Hoisted: these were being recomputed up to four times per blade.
    const float uBodyMin = WorldToColumn(boxMin.x, terrainBody);
    const float uBodyMax = WorldToColumn(boxMax.x, terrainBody);
    const float uMin = uBodyMin - disturbPadding;
    const float uMax = uBodyMax + disturbPadding;
    if (uMax < 0.0f || uMin > static_cast<float>(m_Width)) return;

    float sy = SafeScale(terrainBody.transform.scale.y);
    float localTopWorldY = terrainBody.transform.position.y - terrainBody.size.y * 0.5f * sy;

    float speed = std::abs(disturber.velocity.x);
    float bodyCenterX = box.center.x;

    for (GrassBlade& blade : m_Blades) {
        float col = static_cast<float>(blade.column) + 0.5f;
        if (col < uMin || col > uMax) continue;

        // Vertical gate: a character jumping over the grass or standing on a
        // ledge above it shouldn't part it. Compared against the blade's own
        // tip/root, which matters on uneven ground.
        float tipWorldY = localTopWorldY + static_cast<float>(blade.rootY - blade.height + 1) * sy;
        float rootWorldY = localTopWorldY + static_cast<float>(blade.rootY + 1) * sy;
        float padY = disturbPadding * sy;
        if (boxMax.y < tipWorldY - padY || boxMin.y > rootWorldY + padY) continue;

        // 1 inside the body's x-span, tapering to 0 across the padding either
        // side, so grass at the edge of a footstep only twitches.
        float overshoot = 0.0f;
        if (col < uBodyMin) overshoot = uBodyMin - col;
        else if (col > uBodyMax) overshoot = col - uBodyMax;
        float falloff = disturbPadding > 0.0f ? std::clamp(1.0f - overshoot / disturbPadding, 0.0f, 1.0f) : 1.0f;
        if (falloff <= 0.0f) continue;

        // Moving sweeps the grass along; standing splays it away, so a character
        // at rest sits in a parted patch instead of a field that snaps upright
        // the instant they stop walking.
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

            // What makes the erase exact rather than destructive: a blade leaning
            // over a neighbouring column can sit above that column's dirt, so only
            // ever clear a pixel that is air in the column it landed in.
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
            // t is 0 at the root, 1 at the tip, and the offset is t^2: a real
            // blade pivots at its base, so the bottom barely moves while the tip
            // carries the displacement. Linear looks like the blade sliding.
            float t = static_cast<float>(seg + 1) / height;
            float offset = blade.bend * t * t;

            int dx = static_cast<int>(std::lround(offset));

            // A t^2 curve on a hard-bent blade can round adjacent segments to
            // offsets 2 apart, which at one texel wide leaves a diagonal gap --
            // the blade breaks into a stub and a floating green pixel. Clamping
            // to +/-1 per segment keeps it 8-connected root to tip. Costs a
            // little bend range on the tallest blades, which is invisible.
            dx = std::clamp(dx, prevOffset - 1, prevOffset + 1);
            dx = std::clamp(dx, -kMaxBladeHeight, kMaxBladeHeight);
            prevOffset = dx;
            blade.lastOffset[seg] = static_cast<int8_t>(dx);

            int px = blade.column + dx;
            int py = blade.rootY - seg;
            if (px < 0 || px >= m_Width || py < 0) continue;
            if (static_cast<float>(py) >= m_SurfaceY[static_cast<size_t>(px)]) continue;

            // Tips lighter than roots is the cheapest way to make 3-5 pixels read
            // as a blade with a direction rather than a tally mark; the per-blade
            // tint keeps the field from looking like one repeated stamp.
            float mix = std::clamp(blade.tint * 0.55f + t * 0.5f, 0.0f, 1.0f);
            sprite.SetPixel(px, py, Lerp(grassDark, grassLight, mix));
        }
        blade.drawn = true;
    }
}

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

    // At most two passes: the first can shove the body sideways out of a rise it
    // may not climb, the second lands it on whatever it now stands over. Bounded
    // rather than looped to convergence on purpose -- one horizontal and one
    // vertical correction is all a frame's movement needs, and a convergence loop
    // against a noisy heightfield will find a corner to oscillate in.
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

        // Highest surface (smallest world Y, since +y is down) anywhere under the
        // body, not under its centre: a box rests on the tallest thing beneath
        // it, and sampling the centre lets a bump under one corner get swallowed.
        float highest = localTopWorldY + m_SurfaceY[static_cast<size_t>(c0)] * sy;
        for (int c = c0 + 1; c <= c1; ++c) {
            highest = std::min(highest, localTopWorldY + m_SurfaceY[static_cast<size_t>(c)] * sy);
        }

        float penetration = boxMax.y - highest;
        if (penetration <= 0.0f) return resolved; // airborne / clear of the ground

        // Fully below the surface is deliberately not resolved: there is no
        // correct way out of solid ground for a heightfield, and pushing up would
        // teleport a body that wandered in from a chunk's open end to the top of
        // the world. Anything that belongs under the terrain stays there.
        if (boxMin.y >= highest) return resolved;

        if (penetration <= maxStepHeight) {
            body.ApplyCollisionCorrection(Vector2(0.0f, -penetration));
            return true; // vertical resolution is terminal -- we're standing on it
        }

        if (pass == 1) return resolved; // already shoved once this call; don't ping-pong

        // Too tall to step onto: treat those columns as a wall and resolve along
        // X. Direction follows travel, or the shorter way out when standing still.
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