#pragma once

// The numeric kernels behind objects/leg_rig.lua. Pure functions, no
// state, no ownership -- every knob that decides POLICY (module sizes,
// stride, stance ratio, which side the knee bends, where the hips sit)
// stays in Lua and is passed in. This is the arithmetic only.
//
// All of it is TEXEL space with +Y down, and everything that lands on a
// joint is rounded half-up (floor(v + 0.5)) -- deliberately the same
// rule quad.vert's u_PixelSnap uses, so a position computed here and a
// position snapped by the vertex stage never disagree about which texel
// they mean.
namespace LegIK {

struct Joints {
    float kneeX = 0.0f, kneeY = 0.0f;
    float ankleX = 0.0f, ankleY = 0.0f;
};

// Closed-form two-bone IK (law of cosines), hip -> knee -> ankle.
// `side` is the signed side the knee is pushed toward (bend * facing in
// Lua terms): +1 puts it in front of the actor, -1 behind it.
//
// The knee is rounded to a whole texel and the ankle is then RE-ANCHORED
// onto that rounded knee, so the shin's drawn length matches its
// authored length after quantization -- otherwise rounding the knee
// silently stretches or shortens the bone by up to a texel, which reads
// as the shin breathing while you walk.
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

// `phase` in [0,1). The first (1 - stanceRatio) is SWING, the rest is
// STANCE. Stance sweep is EXACTLY LINEAR and must stay that way -- it is
// the half of the cycle where the foot touches the world, and any easing
// there becomes visible sliding. All sweep shaping is in the swing.
//
// load/push/roll model the three rockers of a real stance phase:
// weight acceptance (heel), single support (flat), pre-swing (toe). The
// width of the two rocker windows is DERIVED from stanceRatio -- it is
// exactly the double-support overlap, (2*stanceRatio - 1)/stanceRatio --
// so there is no second knob that can fall out of sync with the gait.
// For a two-legged opposed gait the two feet's loads sum to 1 by
// construction, which is what lets the rig treat load as a weight share
// without normalizing.
GaitSample SampleGait(float phase, float stanceRatio, int swingFrames);

// Largest sideways offset the knee can ever reach from the hip->ankle
// line, sampled across d in [dMin, L1 + L2]. The closed-form bound
// (L1 * L2 / |L1 - L2|) is correct but loose enough to overestimate the
// stock player's leg canvas by more than half, and every texel it saves
// is a texel the lighting pass doesn't walk on every light every frame.
// Called once at rig construction, not per frame.
float KneeBulge(float L1, float L2, float dMin);

// Framerate-independent exponential approach -- same shape as
// Camera2D::Follow and RigidBody2D's drag, and for the same reason: a
// plain current + (target - current) * rate * dt overshoots, and
// oscillates once rate * dt > 1.
float Approach(float current, float target, float rate, float dt);

} // namespace LegIK