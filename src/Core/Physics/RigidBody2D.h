#pragma once
#include "../Math/Vector2.h"
#include "../Math/Transform2D.h"
#include "../Math/Color.h"
#include "CollisionShape2D.h"

class Shader;
class PlayerActorConfig;

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

    Vector2 velocity;
    float mass = 1.0f;
    float drag = 0.0f; // By simple Linear damping, 0 = none

    float angularVelocity = 0.0f;

    void AddForce(const Vector2& force) {m_ForceAccum += force;}
    
    // Advances Velocity from Accumulated force, then advances the transorms position
    // This is form the velocity, and called once per step with a fixed frame or DeltaTime
    void Integrate(float dt) {
        if (mass > 0.0f)
            velocity += (m_ForceAccum / mass) * dt;
        
        if (drag > 0.0f)
            velocity *= (1.0f - drag * dt);

        transform.position += velocity * dt;
        m_ForceAccum = Vector2::Zero();
    }

    // True if both bodies have a collisionShape attached and those shapes
    // currently overlap in world space. Returns false (not an error) if
    // either body has no shape set yet.
    bool CollidesWith(const RigidBody2D& other) const {
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
    // either one was free to move).
    bool ResolveCollisionWith(RigidBody2D& other) {
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
        return true;
    }

private:
    Vector2 m_ForceAccum;
};