#pragma once
#include "../Math/Color.h"

// Marks a RigidBody2D as a dynamic light source -- attached the same way
// PlayerActorConfig/CollisionShape2D/Camera2D are (RigidBody2D::
// lightEmitter, a non-owning pointer; instance owned by ActorRegistry,
// created via LightEmitterConfig.new() from Lua -- see ScriptBindings.cpp).
//
// Consumed entirely by LightingSystem (see LightingSystem.h) once a frame.
// Nothing here is baked or cached -- move the owning body (or whatever
// it's shining on) and the very next frame's pass reflects it. The owning
// body's transform.position IS the light's world-space origin, same "the
// RigidBody2D is the actor" convention Camera2D already uses.
class LightEmitterConfig {
public:
    enum class Type { Point, Cone };

    // Point = radiates in every direction (a torch, a campfire, the sun).
    // Cone = a directional wedge, see coneAngleRad/coneDirectionRad below
    // (a spotlight, a flashlight).
    Type type = Type::Point;

    // Tint mixed into nearby solid pixels -- see PixelSprite::
    // AccumulateLightTint. Defaults to a warm torch orange.
    Color color = Color(1.0f, 0.55f, 0.15f, 1.0f);

    // How far the light reaches, in world pixels.
    float radius = 180.0f;

    // Tint strength multiplier at the light's own position (distance 0).
    // ~1.0 reads as a normal torch; push toward 2+ for something closer
    // to "standing next to the sun". 0 effectively turns the light off
    // without detaching it.
    float brightness = 1.0f;

    // Shapes the radial falloff curve: strength = brightness *
    // (1 - dist/radius)^falloffExponent. 1.0 = linear falloff, 2.0
    // (default) = a softer, more natural "brightest right up close"
    // curve; higher values make a tighter hotspot that drops off faster
    // near the edge of `radius`.
    float falloffExponent = 2.0f;

    // Cone-only, both stored in RADIANS (Transform2D.h's rule: "Degrees/
    // Radians conversion happens at the Lua boundary, not here" -- see
    // ScriptBindings.cpp's GetConeAngle/SetConeAngle and
    // GetConeDirection/SetConeDirection, which convert to/from degrees).
    float coneAngleRad = 60.0f * 0.01745329252f;    // FULL angle -- 60 deg default = 30 either side of coneDirectionRad
    float coneDirectionRad = 0.0f;                  // 0 = owning body's/world's +X, same convention as Transform2D::rotation

    // If true (default), coneDirectionRad is added ON TOP OF the owning
    // body's transform.rotation every frame -- the spotlight turns with
    // whatever it's mounted on (e.g. a lantern the player carries facing
    // wherever they're aimed). If false, coneDirectionRad is an absolute
    // world-space angle, ignoring the body's own rotation entirely -- a
    // fixed spotlight bolted to a wall.
    bool useOwnerRotation = true;

    // Flicker -- purely a per-frame brightness/color jitter, sampled once
    // per light per LightingSystem::Update() call (never per-pixel, so a
    // single frame's lit region never has a hard seam from its own
    // flicker). Aimed squarely at the "campfire" ask: brightness wobbles
    // by up to +/-flickerIntensityAmount (a fraction of `brightness`),
    // and color drifts toward `color + flickerColorShift` on the upswing
    // -- the default nudges warm orange toward yellow at the brightest
    // instants, roughly what a real flame does.
    bool flicker = false;
    float flickerSpeed = 6.0f;              // how fast the jitter cycles, roughly Hz
    float flickerIntensityAmount = 0.25f;   // +/- fraction of `brightness`
    Color flickerColorShift = Color(0.25f, 0.15f, 0.0f, 0.0f); // added toward `color` at the peak of the jitter
};