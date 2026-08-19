#pragma once
#include <cmath>
#include "../Math/Vector2.h"
#include "../Math/Transform2D.h"
#include "../Math/Color.h"
#include "CollisionShape2D.h"
#include "../Gameplay/Camera2D.h"

class Shader;
class PlayerActorConfig;
class PixelSprite;
class LightEmitterConfig;

// Minimal liner-motion body: velocity + accumulated force, integrated with
// semi-implicit Euler. No collision response (for now...) -- collisionShape
// below only gives you a yes/no overlap test via CollidesWith, not any kind
// of resolution/bounce.
class RigidBody2D {
public:
    Transform2D transform;
    Vector2 size = Vector2(50.0f, 50.0f);
    Color color = Color::White();

    // Non-owninf, Renderer2Ds bult in flat color shader
    // lifetime is owned by ActorRegistry
    Shader* shader = nullptr;

    // Non-owning, same lifetime convention as `shader` above -- both are
    // owned and kept alive by ActorRegistry.
    CollisionShape2D* collisionShape = nullptr;
    PlayerActorConfig* playerConfig = nullptr;

    // Non-owning, same lifetime convention as the pointers above. When
    // set, this body IS a camera -- ActorRegistry::GetActiveCamera() will
    // find it (if Camera2D::active) and Renderer2D will map world-space
    // draws relative to this body's transform.position and the Camera2D's
    // viewportSize. See UpdateCamera() below for the lerp-follow step.
    Camera2D* camera = nullptr;

    // Non-owning, same lifetime convention as `shader`/`collisionShape`
    // above. When set, DrawBody() (see ScriptBindings.cpp) draws this
    // body's texture instead of a flat color quad, flushing any pending
    // SetPixel/PunchCircle edits first.
    PixelSprite* sprite = nullptr;

    // Non-owning, same lifetime convention as the pointers above. When
    // set, this body IS a dynamic light source -- LightingSystem::Update()
    // (see Core/Gameplay/LightingSystem.h) finds it every frame the same
    // way ActorRegistry::GetPlayerActor() finds playerConfig, and radially
    // tints nearby sprite-backed bodies' solid pixels according to
    // whatever's configured on it.
    LightEmitterConfig* lightEmitter = nullptr;

    // Plain bool, not a pointer-tag like the above -- there's no per-
    // instance configuration yet (see LightEmitterConfig.h's header
    // comment for why light_emissive IS a config object and this isn't).
    // When true, LightingSystem's raymarch stops dead at this body's
    // first solid pixel along a given light ray instead of passing
    // through it -- border walls, terrain, anything meant to cast a hard
    // shadow. A body can be both lightBlocking AND have a sprite/
    // lightEmitter of its own; blocking only affects OTHER lights' rays
    // passing through this body, never its own glow.
    bool lightBlocking = false;

    Vector2 velocity;
    float mass = 1.0f;
    float drag = 0.0f; // By simple Linear damping, 0 = none

    float angularVelocity = 0.0f;

    // Engine-wide gravity acceleration (px/s^2), shared by every
    // RigidBody2D -- not a per-instance value. Defaults to zero so a
    // script that never touches this (e.g. a future top-down game) sees
    // no behavior change; a sidescroller opts in via SetGravity/Lua's
    // Physics.SetGravity. Applied in Integrate() below to any body with
    // mass > 0 -- same "mass <= 0 is static/immovable" convention already
    // used for m_ForceAccum here and for the wall in ResolveCollisionWith.
    static void SetGravity(const Vector2& gravity) {s_Gravity = gravity;}
    static Vector2 GetGravity() {return s_Gravity;}

    void AddForce(const Vector2& force) {m_ForceAccum += force;}

    // True if a ResolveCollisionWith/ResolveWindowBounds call made since
    // the last Integrate() pushed this body upward out of an overlap --
    // i.e. it's resting on something below it as of this frame. Meant for
    // gating things like a jump (only allowed while grounded) from Lua;
    // see the comment on Integrate() below for the reset timing.
    bool IsGrounded() const {return m_Grounded;}

    // Advances Velocity from Accumulated force, then advances the transorms position
    // This is form the velocity, and called once per step with a fixed frame or DeltaTime
    void Integrate(float dt) {
        // Reset here, not in ResolveCollisionWith/ResolveWindowBounds --
        // this makes Integrate() the start of "this frame's" grounded
        // state. The usual per-frame order (Integrate(), then resolve
        // collisions against the floor/walls) means IsGrounded() during
        // THIS Update() call still reflects last frame's resolution
        // (correct for "am I allowed to jump right now"), and gets
        // refreshed by the resolve calls that follow before next frame.
        m_Grounded = false;

        if (mass > 0.0f) {
            velocity += (m_ForceAccum / mass) * dt;
            // Gravity is an acceleration (F=mg, a=F/m=g -- mass cancels
            // out), not a force, so it's added directly to velocity
            // rather than routed through m_ForceAccum/mass.
            velocity += s_Gravity * dt;
        }

        // Exponential decay, framerate-independent -- same shape as
        // Camera2D::Follow's own dt-scaled exponential. The old
        // `velocity *= (1.0f - drag * dt)` linear form flips sign once
        // `drag * dt > 1.0` (a high drag value, a low framerate, or a
        // hitch) instead of just damping toward zero -- e.g. drag=20 at
        // 20fps (dt=0.05) computes a factor of 0.0, and anything
        // slightly higher goes negative, reversing the body's velocity
        // instead of slowing it down. std::exp(-drag * dt) can't cross
        // zero, so it always damps, never reverses, regardless of dt.
        if (drag > 0.0f)
            velocity *= std::exp(-drag * dt);

        transform.position += velocity * dt;
        m_ForceAccum = Vector2::Zero();
    }

    // If a Camera2D is attached, lerps this body's own position toward
    // camera->followTarget (a no-op otherwise -- no camera, no target, or
    // followSmoothing <= 0). Mirrors Integrate()'s shape: a plain per-frame
    // step method on the body, called explicitly from Lua once a frame
    // (e.g. cameraBody:UpdateCamera(deltaTime)), same as Integrate()/
    // ResolveWindowBounds() already are. Split out from Follow() itself
    // (which lives on Camera2D, see its .cpp) only because that's where
    // RigidBody2D's full definition is actually needed.
    void UpdateCamera(float dt) {
        if (camera) camera->Follow(*this, dt);
    }

    // True if both bodies have a collisionShape attached and those shapes
    // currently overlap in world space. Returns false (not an error) if
    // either body has no shape set yet. Also false if `other` IS this
    // body (same address) -- a shape can never meaningfully overlap
    // itself, and without this guard a caller that (accidentally or not)
    // includes a body in its own "solids to check against" list -- e.g.
    // Lua building one shared `solids` table and passing it to every
    // mover's own Update(), including itself -- would get a spurious
    // "yes, overlapping" back every single frame.
    bool CollidesWith(const RigidBody2D& other) const {
        if (this == &other) return false;
        if (!collisionShape || !other.collisionShape) return false;
        return CollisionShape2D::Intersects(*collisionShape, transform, *other.collisionShape, other.transform);
    }

    // Pushes both bodies apart just enough to stop overlapping (positional
    // correction only -- no velocity/momentum transfer, so a pushed object
    // won't keep sliding once you stop touching it). Box shapes only for
    // now, same limitation as CollisionShape2D::ComputeBoxSeparation.
    //
    // A body with mass <= 0 is treated as immovable -- same convention
    // Integrate() already uses for "never accelerates" -- so give a wall
    // mass = 0 and it won't budge, while a mass > 0 body gets shoved.
    // Returns true if the bodies were actually overlapping (whether or not
    // either one was free to move). Same self-collision guard as
    // CollidesWith() above, and for the same reason -- without it, a body
    // resolving against itself computes a nonzero "correction" (its own
    // AABB always fully overlaps itself), and because `other` here IS
    // `this`, the position correction below cancels out to a no-op by
    // sheer aliasing coincidence -- but the velocity.x/y zeroing a few
    // lines down does NOT cancel, so the body's velocity on one axis gets
    // silently reset to zero every frame it's included in its own solids
    // list (e.g. gravity never actually accumulates -- it just creeps at
    // a constant per-frame increment instead of accelerating).
    bool ResolveCollisionWith(RigidBody2D& other) {
        if (this == &other) return false;
        if (!collisionShape || !other.collisionShape) return false;

        Vector2 correction;
        if (!CollisionShape2D::ComputeBoxSeparation(*collisionShape, transform, *other.collisionShape, other.transform, correction))
            return false;

        float invMassSelf = (mass > 0.0f) ? 1.0f / mass : 0.0f;
        float invMassOther = (other.mass > 0.0f) ? 1.0f / other.mass : 0.0f;
        float totalInvMass = invMassSelf + invMassOther;
        if (totalInvMass <= 0.0f) return true; // both immovable -- nothing to correct

        transform.position += correction * (invMassSelf / totalInvMass);
        other.transform.position -= correction * (invMassOther / totalInvMass);

        // Zero self's velocity on whichever axis just got corrected --
        // same convention ResolveWindowBounds already uses below. Without
        // this, a resting body (e.g. the player standing on a floor made
        // of another RigidBody2D rather than the window edge) gets
        // positionally clamped every frame but keeps accelerating under
        // gravity underneath that clamp, forever, invisibly -- harmless
        // while the correction keeps catching it, but the first frame it
        // doesn't (an edge, a gap, a thin platform) that velocity is still
        // there and causes a teleport/tunnel instead of a normal fall.
        // Only self's velocity is touched, matching the self-vs-other
        // asymmetry the rest of this method already has -- `other` is
        // typically immovable (mass <= 0) terrain that never calls
        // Integrate() anyway, so its velocity is inert regardless.
        if (correction.x != 0.0f) velocity.x = 0.0f;
        if (correction.y != 0.0f) velocity.y = 0.0f;

        // correction.y < 0 means THIS body got pushed up out of the
        // overlap -- i.e. `other` was underneath it. Same convention
        // ResolveWindowBounds uses below. Only self's grounded state is
        // updated (matching the self-vs-other asymmetry the rest of this
        // method already has) -- call it from whichever body cares about
        // standing on the other, e.g. player:ResolveCollisionWith(floor).
        if (correction.y < 0.0f) m_Grounded = true;

        return true;
    }
    
    // Clamps this body fully inside [0,0]..[windowWidth,windowHeight] -- an
    // absolute world boundary rather than a pushable collision, so unlike
    // ResolveCollisionWith this ignores mass entirely (a wall placed
    // half-offscreen on purpose still gets clamped if you call this on it;
    // in practice you'll only call it on bodies you actually want confined,
    // typically just the player).
    //
    // Uses the attached CollisionShape2D's world AABB if one is set
    // (GetWorldAABB already handles Circle by giving its bounding square),
    // otherwise falls back to a box centered on `size`. Zeroes velocity on
    // whichever axis got clamped, same "stop dead at the wall" behavior
    // ResolveCollisionWith gives you against a mass<=0 body, so the player
    // doesn't keep pushing into the edge and jitter every frame.
    //
    // Returns true if a clamp happened this call.
    bool ResolveWindowBounds(float windowWidth, float windowHeight) {
        AABB box = collisionShape
            ? collisionShape->GetWorldAABB(transform)
            : AABB{transform.position, size * 0.5f};

        Vector2 min = box.Min();
        Vector2 max = box.Max();
        Vector2 correction = Vector2::Zero();

        if (min.x < 0.0f)               correction.x = -min.x;
        else if (max.x > windowWidth)   correction.x = windowWidth - max.x;

        if (min.y < 0.0f)               correction.y = -min.y;
        else if (max.y > windowHeight)  correction.y = windowHeight - max.y;

        if (correction.x == 0.0f && correction.y == 0.0f) return false;

        transform.position += correction;
        if (correction.x != 0.0f) velocity.x = 0.0f;
        if (correction.y != 0.0f) velocity.y = 0.0f;

        // Same grounded convention as ResolveCollisionWith: correction.y
        // < 0 means we were pushed up off the window's bottom edge, i.e.
        // resting on the "floor" the window bounds represent.
        if (correction.y < 0.0f) m_Grounded = true;

        return true;
    }

private:
    Vector2 m_ForceAccum;
    bool m_Grounded = false;
    inline static Vector2 s_Gravity = Vector2::Zero();
};