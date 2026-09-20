#include "LegIK.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

inline float RoundTexel(float v) { return std::floor(v + 0.5f); }

inline float Clamp(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

} // namespace

namespace LegIK {

Joints SolveTwoBone(float hipX, float hipY,
                    float ankleX, float ankleY,
                    float L1, float L2, float side) {
    float dx = ankleX - hipX;
    float dy = ankleY - hipY;
    float d = std::sqrt(dx * dx + dy * dy);
    if (d < 1e-5f) { dx = 0.0f; dy = 1.0f; d = 1.0f; }

    // Clamped just inside both singularities: exactly at full extension
    // (or full fold) the knee's offset from the hip->ankle line is zero
    // and its side becomes numerically undecided, which reads as a
    // one-frame knee flip.
    const float dClamped = Clamp(d, std::fabs(L1 - L2) + 0.01f, L1 + L2 - 0.01f);

    const float ux = dx / d;
    const float uy = dy / d;

    const float a = (L1 * L1 - L2 * L2 + dClamped * dClamped) / (2.0f * dClamped);
    const float h = std::sqrt(std::max(0.0f, L1 * L1 - a * a));

    // (uy, -ux) is the perpendicular pointing toward +x when the leg
    // hangs straight down, so `side` selects front/back of the actor.
    const float nx =  uy * side;
    const float ny = -ux * side;

    Joints out;
    out.kneeX = RoundTexel(hipX + ux * a + nx * h);
    out.kneeY = RoundTexel(hipY + uy * a + ny * h);
    out.ankleX = ankleX;
    out.ankleY = ankleY;

    if (L2 > 0.0f) {
        const float kx = ankleX - out.kneeX;
        const float ky = ankleY - out.kneeY;
        const float klen = std::sqrt(kx * kx + ky * ky);
        if (klen > 1e-5f) {
            out.ankleX = RoundTexel(out.kneeX + kx / klen * L2);
            out.ankleY = RoundTexel(out.kneeY + ky / klen * L2);
        }
    }

    return out;
}

GaitSample SampleGait(float phase, float stanceRatio, int swingFrames) {
    GaitSample out;

    const float swingSpan = 1.0f - stanceRatio;
    if (swingSpan <= 0.0f) {
        out.sweep = 1.0f - 2.0f * phase;
        out.load  = 1.0f;
        return out;
    }

    if (phase < swingSpan) {
        float t = phase / swingSpan;
        if (swingFrames > 0) {
            t = std::floor(t * static_cast<float>(swingFrames) + 0.5f) / static_cast<float>(swingFrames);
        }
        const float eased = t * t * (3.0f - 2.0f * t);
        out.sweep = -1.0f + 2.0f * eased;
        out.lift  = std::sin(kPi * std::pow(t, 0.8f));
        // Unwinds from toe-off back to heel-first, so the foot arrives
        // at its next plant already tipped for the strike.
        out.roll  = 1.0f - 2.0f * eased;
        return out;
    }

    const float t = (phase - swingSpan) / stanceRatio;
    out.sweep = 1.0f - 2.0f * t;

    float ds = (2.0f * stanceRatio - 1.0f) / stanceRatio;
    ds = Clamp(ds, 0.06f, 0.45f);

    if (t < ds) {                       // weight acceptance -- heel rocker
        const float u = t / ds;
        out.load = u * u * (3.0f - 2.0f * u);
        out.roll = -1.0f + out.load;
    } else if (t > 1.0f - ds) {         // pre-swing -- toe rocker
        const float u = (t - (1.0f - ds)) / ds;
        const float e = u * u * (3.0f - 2.0f * u);
        out.load = 1.0f - e;
        out.push = e;
        out.roll = e;
    } else {                            // single support -- flat
        out.load = 1.0f;
    }

    return out;
}

float KneeBulge(float L1, float L2, float dMin) {
    if (L1 <= 0.0f || L2 <= 0.0f) return 0.0f;

    dMin = std::max(dMin, std::fabs(L1 - L2));
    const float dMax = L1 + L2;
    if (dMax <= dMin) return 0.0f;

    float peak = 0.0f;
    constexpr int kSamples = 64;
    for (int i = 0; i <= kSamples; ++i) {
        const float d = dMin + (dMax - dMin) * (static_cast<float>(i) / kSamples);
        if (d <= 1e-5f) continue;
        const float a = (L1 * L1 - L2 * L2 + d * d) / (2.0f * d);
        const float hh = L1 * L1 - a * a;
        if (hh > 0.0f) peak = std::max(peak, std::sqrt(hh));
    }
    return peak;
}

float Approach(float current, float target, float rate, float dt) {
    if (rate <= 0.0f) return target;
    return current + (target - current) * (1.0f - std::exp(-rate * dt));
}

} // namespace LegIK