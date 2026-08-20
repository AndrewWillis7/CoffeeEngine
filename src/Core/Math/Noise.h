#pragma once
#include <cmath>
#include <cstdint>

// Deterministic, allocation-free, seedable noise -- the procedural-content
// counterpart to the rest of this folder's plain math types. Header-only
// and stateless for the same reason Vector2/AABB are: every function here
// is a pure f(inputs) -> float, so it can be called from generation code,
// gameplay code, or a shader-parameter tweak without anyone owning an
// instance or worrying about call order.
//
// Everything is VALUE noise (hash the integer lattice, interpolate
// between corners), not Perlin/Simplex gradient noise. Value noise is
// cheaper, has no patent/attribution baggage, and its slightly "blockier"
// character is a feature rather than a flaw for chunky pixel art -- the
// terrain surface and dirt speckle both want visible structure at the
// texel scale, not the ultra-smooth blobs gradient noise gives you.
//
// CONVENTIONS
//   - Every *01 function returns [0, 1]. Every *Signed function returns
//     [-1, 1]. Nothing here returns an unbounded value, so callers can
//     always scale by a plain amplitude without clamping first.
//   - `seed` is an int, threaded explicitly through every call rather
//     than held in global state -- two systems sampling noise in the same
//     frame must not be able to perturb each other, and the same seed
//     must always regenerate the same world (hot-reload a script and the
//     terrain comes back identical).
//   - Frequency is baked into the CALLER'S coordinate, not passed as a
//     parameter: sample Value1D01(x * 0.02f, seed), not
//     Value1D01(x, 0.02f, seed). Keeps the API small and makes
//     multi-octave code (below) read as a plain coordinate scale.
namespace Noise {

// Integer avalanche (the murmur3 finalizer's shape, different constants).
// Every input bit affects every output bit, so adjacent lattice cells --
// which differ by exactly 1 -- produce completely unrelated values. A
// weaker mixer here shows up as diagonal banding in 2D, which on a
// terrain surface reads as an obviously artificial repeating ridge.
inline uint32_t HashU32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// Two mixing rounds rather than one xor-and-mix of all three inputs:
// folding x, y and seed together in a single round leaves visible
// correlation along the x == y diagonal.
inline uint32_t Hash2(int x, int y, int seed) {
    uint32_t h = HashU32(static_cast<uint32_t>(seed) * 0x9e3779b1u + 0x165667b1u);
    h = HashU32(h ^ (static_cast<uint32_t>(x) * 0x85ebca77u));
    h = HashU32(h ^ (static_cast<uint32_t>(y) * 0xc2b2ae3du));
    return h;
}

// Uncorrelated white noise at an integer lattice point, [0, 1). No
// interpolation at all -- this is the "scatter a pebble here or not"
// primitive, deliberately harsh where Value2D01 below is smooth.
inline float Hash01(int x, int y, int seed) {
    return static_cast<float>(Hash2(x, y, seed)) * (1.0f / 4294967296.0f);
}

inline float Hash01(int x, int seed) { return Hash01(x, 0, seed); }

// Smoothstep, used as the interpolation curve between lattice corners.
// Plain linear interpolation would leave a visible crease (a first-
// derivative discontinuity) at every integer coordinate, which on a
// terrain surface reads as evenly-spaced kinks.
inline float Fade(float t) { return t * t * (3.0f - 2.0f * t); }

// 1D value noise -- the terrain SURFACE primitive. One smooth wobble per
// unit of x.
inline float Value1D01(float x, int seed) {
    float fx = std::floor(x);
    int ix = static_cast<int>(fx);
    float t = Fade(x - fx);

    float a = Hash01(ix, 0, seed);
    float b = Hash01(ix + 1, 0, seed);
    return a + (b - a) * t;
}

// 2D value noise -- the terrain INTERIOR primitive (dirt mottling), and
// anything else that wants smooth variation across an area rather than
// along a line.
inline float Value2D01(float x, float y, int seed) {
    float fx = std::floor(x);
    float fy = std::floor(y);
    int ix = static_cast<int>(fx);
    int iy = static_cast<int>(fy);
    float tx = Fade(x - fx);
    float ty = Fade(y - fy);

    float c00 = Hash01(ix,     iy,     seed);
    float c10 = Hash01(ix + 1, iy,     seed);
    float c01 = Hash01(ix,     iy + 1, seed);
    float c11 = Hash01(ix + 1, iy + 1, seed);

    float top = c00 + (c10 - c00) * tx;
    float bottom = c01 + (c11 - c01) * tx;
    return top + (bottom - top) * ty;
}

inline float Value1DSigned(float x, int seed) { return Value1D01(x, seed) * 2.0f - 1.0f; }
inline float Value2DSigned(float x, float y, int seed) { return Value2D01(x, y, seed) * 2.0f - 1.0f; }

// Fractal Brownian motion -- sum `octaves` copies of the base noise, each
// one `lacunarity` times finer and `gain` times weaker than the last, and
// normalize the result back into [0, 1] by the total amplitude actually
// summed (so the return range doesn't quietly depend on the octave count).
//
// This is what turns a single smooth wobble into terrain that reads as
// natural: octave 1 gives the broad hills, octave 2 the bumps on those
// hills, octave 3 the texel-scale roughness on those bumps. Two or three
// octaves is usually plenty at pixel-art scale -- past the point where an
// octave's wavelength drops below ~2 texels it's just adding per-pixel
// hash noise, which the eye reads as dither, not detail.
//
// NOTE ON RANGE: because this is a normalized SUM of independent samples,
// its output clusters toward 0.5 far more than a single octave does --
// three octaves rarely leaves [0.2, 0.8] in practice. That's correct
// fractal behavior, but it means an amplitude picked against a single
// octave will look roughly half as strong here. Pick amplitudes by
// looking at the result, not by assuming the full [0,1] swing.
//
// Each octave uses a DIFFERENT derived seed rather than just a scaled
// coordinate; reusing one seed at 2x frequency makes every octave line up
// at the integer lattice, producing self-similar spikes at regular
// intervals instead of irregular terrain.
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

// Rounds `v` (expected [0, 1]) DOWN onto `steps` evenly-spaced levels --
// the same posterizing knob LightEmitterConfig::toneSteps applies to
// light falloff, factored out here so terrain shading can use it too.
// steps <= 1 is a pass-through (no quantization), matching toneSteps' own
// "0 means off" convention.
//
// Why terrain wants this at all: smooth FBM across a dirt fill produces a
// continuous gradient, and a continuous gradient rendered at one texel
// per pixel reads as an out-of-place airbrush against hand-authored
// pixel art. Snapping to 4-6 levels gives distinct, readable dirt "tones"
// -- the same thing a pixel artist does by hand when they pick a 4-color
// ramp instead of blending.
inline float Quantize01(float v, int steps) {
    if (steps <= 1) return v;
    float s = static_cast<float>(steps);
    float q = std::floor(v * s) / (s - 1.0f);
    return q > 1.0f ? 1.0f : q;
}

} // namespace Noise