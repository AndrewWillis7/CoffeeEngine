#pragma once

// The numeric kernels behind objects/leg_rig.lua: pure functions, no state, no
// ownership. Every policy knob -- module sizes, stride, stance ratio, which way
// the knee bends, where the hips sit -- stays in Lua and is passed in.
//
// Texel space with +Y down. Anything landing on a joint is rounded half-up,
// the same rule quad.vert's u_PixelSnap uses, so a position computed here and
// one snapped by the vertex stage never disagree about which texel they mean.
namespace LegIK {

struct Joints {
    float kneeX = 0.0f, kneeY = 0.0f;
    float ankleX = 0.0f, ankleY = 0.0f;
};

// Closed-form two-bone IK (law of cosines), hip -> knee -> ankle.
// `side` is the signed side the knee is pushed toward (bend * facing in
// Lua terms): +1 puts it in front of the actor, -1 behind it.
//
// The knee rounds to a whole texel and the ankle is then RE-ANCHORED onto it,
// so the shin's drawn length matches its authored length after quantization --
// otherwise rounding stretches the bone by up to a texel and the shin breathes.
Joints SolveTwoBone(float hipX, float hipY,
                    float ankleX, float ankleY,
                    float L1, float L2, float side);

struct GaitSample {
    float sweep = 0.0f; // -1..1, forward/back offset from the hip
    float lift  = 0.0f; //  0..1, height above the ground plane
    float load  = 0.0f; //  0..1, fraction of body weight on this foot
    float push  = 0.0f; //  0..1, heel-off / toe push progress
    float roll  = 0.0f; // -1 heel .. 0 flat .. +1 toe
};

// `phase` in [0,1): the first (1 - stanceRatio) is SWING, the rest STANCE.
// Stance sweep is EXACTLY LINEAR and must stay so -- it is the half of the
// cycle where the foot touches the world, and any easing there reads as
// sliding. All sweep shaping belongs in the swing.
//
// load/push/roll model the three rockers of a real stance: weight acceptance
// (heel), single support (flat), pre-swing (toe). The rocker windows are
// DERIVED from stanceRatio -- exactly the double-support overlap -- so no
// second knob can fall out of sync. For a two-legged opposed gait the two
// loads sum to 1 by construction, so the rig can treat load as a weight share
// without normalizing.
GaitSample SampleGait(float phase, float stanceRatio, int swingFrames);

// Largest sideways offset the knee can reach from the hip->ankle line, sampled
// across d in [dMin, L1 + L2]. The closed form is correct but loose enough to
// more than double the stock leg canvas, and every texel saved is one the
// lighting pass doesn't walk. Called once at construction, not per frame.
float KneeBulge(float L1, float L2, float dMin);

// Framerate-independent exponential approach, same shape and reasoning as
// Camera2D::Follow: the plain lerp form overshoots once rate * dt > 1.
float Approach(float current, float target, float rate, float dt);

} // namespace LegIK