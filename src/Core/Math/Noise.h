#pragma once
#include <cmath>
#include <cstdint>

// Deterministic, allocation-free, seedable value noise. Header-only and
// stateless, so generation code, gameplay code and shader-parameter tweaks can
// all call it without owning an instance or caring about call order.
//
// Value noise (hash the integer lattice, interpolate between corners) rather
// than Perlin/Simplex: cheaper, and its blockier character suits chunky pixel
// art, where visible texel-scale structure is wanted.
//
// Conventions:
//   - *01 returns [0,1], *Signed returns [-1,1]. Nothing is unbounded, so
//     callers can scale by a plain amplitude without clamping.
//   - `seed` is threaded explicitly, never global: two systems sampling in the
//     same frame must not perturb each other, and the same seed must always
//     regenerate the same world across a hot-reload.
//   - Frequency lives in the caller's coordinate -- Value1D01(x * 0.02f, seed).
namespace Noise {

// Integer avalanche (murmur3 finalizer shape, different constants). Every input
// bit affects every output bit, so adjacent lattice cells decorrelate; a weaker
// mixer shows up as diagonal banding, i.e. an artificial repeating ridge.
inline uint32_t HashU32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

namespace detail {
// Hash2 below folds seed, then x, then y in three separate rounds -- one round
// over all three leaves visible correlation along the x == y diagonal. Split
// into stages so lattice sampling can reuse the shared prefixes: a 2x2 corner
// fetch costs 7 rounds this way instead of 12.
inline uint32_t SeedStage(int seed) {
    return HashU32(static_cast<uint32_t>(seed) * 0x9e3779b1u + 0x165667b1u);
}
inline uint32_t XStage(uint32_t seedStage, int x) {
    return HashU32(seedStage ^ (static_cast<uint32_t>(x) * 0x85ebca77u));
}
inline uint32_t YStage(uint32_t xStage, int y) {
    return HashU32(xStage ^ (static_cast<uint32_t>(y) * 0xc2b2ae3du));
}
constexpr float kInv32 = 1.0f / 4294967296.0f;
} // namespace detail

inline uint32_t Hash2(int x, int y, int seed) {
    return detail::YStage(detail::XStage(detail::SeedStage(seed), x), y);
}

// Uncorrelated white noise at a lattice point, [0,1). No interpolation: the
// "scatter a pebble here or not" primitive, deliberately harsh.
inline float Hash01(int x, int y, int seed) {
    return static_cast<float>(Hash2(x, y, seed)) * detail::kInv32;
}

inline float Hash01(int x, int seed) { return Hash01(x, 0, seed); }

// Smoothstep between lattice corners. Linear interpolation would leave a
// first-derivative crease at every integer coordinate -- evenly spaced kinks.
inline float Fade(float t) { return t * t * (3.0f - 2.0f * t); }

// 1D value noise -- the terrain surface primitive.
inline float Value1D01(float x, int seed) {
    float fx = std::floor(x);
    int ix = static_cast<int>(fx);
    float t = Fade(x - fx);

    const uint32_t hs = detail::SeedStage(seed);
    float a = static_cast<float>(detail::YStage(detail::XStage(hs, ix), 0)) * detail::kInv32;
    float b = static_cast<float>(detail::YStage(detail::XStage(hs, ix + 1), 0)) * detail::kInv32;
    return a + (b - a) * t;
}

// 2D value noise -- terrain dirt mottling, and anything wanting smooth
// variation across an area rather than along a line.
inline float Value2D01(float x, float y, int seed) {
    float fx = std::floor(x);
    float fy = std::floor(y);
    int ix = static_cast<int>(fx);
    int iy = static_cast<int>(fy);
    float tx = Fade(x - fx);
    float ty = Fade(y - fy);

    const uint32_t hs = detail::SeedStage(seed);
    const uint32_t hx0 = detail::XStage(hs, ix);
    const uint32_t hx1 = detail::XStage(hs, ix + 1);

    float c00 = static_cast<float>(detail::YStage(hx0, iy))     * detail::kInv32;
    float c10 = static_cast<float>(detail::YStage(hx1, iy))     * detail::kInv32;
    float c01 = static_cast<float>(detail::YStage(hx0, iy + 1)) * detail::kInv32;
    float c11 = static_cast<float>(detail::YStage(hx1, iy + 1)) * detail::kInv32;

    float top = c00 + (c10 - c00) * tx;
    float bottom = c01 + (c11 - c01) * tx;
    return top + (bottom - top) * ty;
}

inline float Value1DSigned(float x, int seed) { return Value1D01(x, seed) * 2.0f - 1.0f; }
inline float Value2DSigned(float x, float y, int seed) { return Value2D01(x, y, seed) * 2.0f - 1.0f; }

// Fractal Brownian motion: sum `octaves` copies, each `lacunarity` times finer
// and `gain` times weaker, normalized by the amplitude actually summed so the
// range doesn't depend on the octave count. Broad hills, bumps on the hills,
// roughness on the bumps. Two or three octaves is plenty at pixel-art scale --
// once an octave's wavelength drops below ~2 texels it reads as dither.
//
// Range caveat: a normalized sum clusters toward 0.5, so three octaves rarely
// leave [0.2, 0.8]. An amplitude tuned against a single octave looks about half
// as strong here.
//
// Each octave takes a different derived seed; reusing one seed at 2x frequency
// lines every octave up at the lattice and gives regularly spaced spikes.
inline float FBM1D01(float x, int octaves, float lacunarity, float gain, int seed) {
    if (octaves < 1) octaves = 1;

    float sum = 0.0f;
    float amplitude = 1.0f;
    float totalAmplitude = 0.0f;
    float frequency = 1.0f;

    for (int i = 0; i < octaves; ++i) {
        sum += Value1D01(x * frequency, seed + i * 1013) * amplitude;
        totalAmplitude += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return totalAmplitude > 0.0f ? sum / totalAmplitude : 0.0f;
}

inline float FBM2D01(float x, float y, int octaves, float lacunarity, float gain, int seed) {
    if (octaves < 1) octaves = 1;

    float sum = 0.0f;
    float amplitude = 1.0f;
    float totalAmplitude = 0.0f;
    float frequency = 1.0f;

    for (int i = 0; i < octaves; ++i) {
        sum += Value2D01(x * frequency, y * frequency, seed + i * 1013) * amplitude;
        totalAmplitude += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return totalAmplitude > 0.0f ? sum / totalAmplitude : 0.0f;
}

inline float FBM1DSigned(float x, int octaves, float lacunarity, float gain, int seed) {
    return FBM1D01(x, octaves, lacunarity, gain, seed) * 2.0f - 1.0f;
}

inline float FBM2DSigned(float x, float y, int octaves, float lacunarity, float gain, int seed) {
    return FBM2D01(x, y, octaves, lacunarity, gain, seed) * 2.0f - 1.0f;
}

// Rounds `v` (expected [0,1]) down onto `steps` evenly spaced levels; steps <= 1
// passes through, matching LightEmitterConfig::toneSteps' "0 means off".
// Smooth FBM across a dirt fill reads as an airbrush against hand-authored
// pixel art; snapping to 4-6 tones is what an artist does by picking a ramp.
inline float Quantize01(float v, int steps) {
    if (steps <= 1) return v;
    float s = static_cast<float>(steps);
    float q = std::floor(v * s) / (s - 1.0f);
    return q > 1.0f ? 1.0f : q;
}

} // namespace Noise
