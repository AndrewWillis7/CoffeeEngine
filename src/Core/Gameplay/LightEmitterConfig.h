#pragma once
#include "../Math/Color.h"

// Marks a RigidBody2D as a dynamic light source; owned by ActorRegistry and
// consumed entirely by LightingSystem once a frame. Nothing is baked, so moving
// the owning body (whose position IS the light's origin) shows up immediately.
class LightEmitterConfig {
public:
    enum class Type { Point, Cone };

    Type type = Type::Point;

    // Tint mixed into nearby solid pixels. Defaults to a warm torch orange.
    Color color = Color(1.0f, 0.55f, 0.15f, 1.0f);

    // Reach in world pixels.
    float radius = 180.0f;

    // Tint strength at distance 0. ~1 reads as a torch; 0 turns the light off
    // without detaching it.
    float brightness = 1.0f;

    // strength = brightness * (1 - dist/radius)^falloffExponent.
    // 1 is linear; higher values tighten the hotspot.
    float falloffExponent = 2.0f;

    // Cone-only, in radians -- the Lua bindings convert to/from degrees.
    float coneAngleRad = 60.0f * 0.01745329252f;    // FULL angle, so 60 deg = 30 either side
    float coneDirectionRad = 0.0f;                  // 0 = +X, matching Transform2D::rotation

    // True: coneDirectionRad is relative to the owning body's rotation, so a
    // carried lantern turns with its holder. False: absolute world angle, for a
    // spotlight bolted to a wall.
    bool useOwnerRotation = true;

    // Per-frame brightness/color jitter, sampled once per light per Update()
    // rather than per pixel, so a frame's lit region never seams. Brightness
    // wobbles by +/-flickerIntensityAmount and color drifts toward
    // color + flickerColorShift on the upswing, which is roughly what flame does.
    bool flicker = false;
    float flickerSpeed = 6.0f;              // roughly Hz
    float flickerIntensityAmount = 0.25f;   // +/- fraction of `brightness`
    Color flickerColorShift = Color(0.25f, 0.15f, 0.0f, 0.0f);

    // Quantizes radial falloff into this many concentric bands. 0 is off (smooth
    // gradient). 3 gives the punchy posterized "video game torch" look -- bright
    // core, mid ring, dim ring. Per-light, not engine-wide, so a moody torch and
    // a clean spotlight can share a scene.
    int toneSteps = 0;
};
