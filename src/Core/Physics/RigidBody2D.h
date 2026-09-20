#pragma once
#include <cmath>
#include <string>
#include "../Math/Vector2.h"
#include "../Math/Transform2D.h"
#include "../Math/Color.h"
#include "CollisionShape2D.h"
#include "../Gameplay/Camera2D.h"

class Shader;
class PlayerActorConfig;
class PixelSprite;
class LightEmitterConfig;
class TerrainChunk;

// Minimal linear-motion body: velocity + accumulated force, integrated with
// semi-implicit Euler. Collision response is positional only -- no bounce.
class RigidBody2D {
public:
    Transform2D transform;
    Vector2 size = Vector2(50.0f, 50.0f);
    Color color = Color::White();

    // Purely a label: nothing in the engine keys off it. Scripts set it so
    // the debug explorer can list "Campfire" instead of "#7 Light".
    std::string name;

    // All non-owning; every one of these is owned by ActorRegistry. Each acts as
    // a capability tag -- setting it is what makes this body a camera, a light,
    // a patch of terrain, and so on.
    Shader* shader = nullptr;
    CollisionShape2D* collisionShape = nullptr;
    PlayerActorConfig* playerConfig = nullptr;
    Camera2D* camera = nullptr;
    PixelSprite* sprite = nullptr;
    LightEmitterConfig* lightEmitter = nullptr;

    // A terrain body deliberately has no collisionShape: its surface is a
    // heightmap, so it resolves through TerrainChunk::ResolveBody (which routes
    // back into ApplyCollisionCorrection). A box on top would be a flat lid
    // fighting the real surface.
    TerrainChunk* terrain = nullptr;

    // Stops LightingSystem's raymarch at this body's first solid pixel. Only
    // affects other lights' rays passing through, never this body's own glow.
    bool lightBlocking = false;

    // Whether Physics.RaycastDown can hit this body. Only consulted when terrain
    // or a collisionShape is attached, so the default means "if you can collide
    // with it, you can stand on it". Clear it for trigger volumes and the like.
    bool raycastTarget = true;

    Vector2 velocity;
    float mass = 1.0f;
    float drag = 0.0f; // Linear damping, 0 = none

    float angularVelocity = 0.0f;

    // Engine-wide, not per-instance. Defaults to zero so a top-down game sees no
    // change; a sidescroller opts in. Applied to any body with mass > 0.
    static void SetGravity(const Vector2& gravity) {s_Gravity = gravity;}
    static Vector2 GetGravity() {return s_Gravity;}

    void AddForce(const Vector2& force) {m_ForceAccum += force;}

    // True if a resolve call since the last Integrate() pushed this body upward
    // out of an overlap, i.e. it is resting on something.
    bool IsGrounded() const {return m_Grounded;}

    void Integrate(float dt) {
        // Cleared here rather than in the resolve calls, so IsGrounded() during
        // this frame's Update() still reflects last frame's resolution -- which
        // is what "am I allowed to jump right now" wants.
        m_Grounded = false;

        if (mass > 0.0f) {
            velocity += (m_ForceAccum / mass) * dt;
            // Gravity is an acceleration, so mass cancels and it bypasses m_ForceAccum.
            velocity += s_Gravity * dt;
        }

        // Exponential decay rather than the linear (1 - drag*dt): that form flips
        // sign once drag*dt > 1 (high drag, low framerate, or a hitch) and
        // reverses the body instead of damping it.
        if (drag > 0.0f)
            velocity *= std::exp(-drag * dt);

        transform.position += velocity * dt;
        m_ForceAccum = Vector2::Zero();
    }

    void UpdateCamera(float dt) {
        if (camera) camera->Follow(*this, dt);
    }

    // False if either body lacks a shape, or if `other` is this body -- callers
    // routinely pass one shared `solids` list to every mover, including itself.
    bool CollidesWith(const RigidBody2D& other) const {
        if (this == &other) return false;
        if (!collisionShape || !other.collisionShape) return false;
        return CollisionShape2D::Intersects(*collisionShape, transform, *other.collisionShape, other.transform);
    }

    // Pushes both bodies apart just enough to stop overlapping; positional only,
    // no momentum transfer. Box shapes only. mass <= 0 means immovable, matching
    // Integrate(). Returns true if they were overlapping at all.
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

        // Zero the corrected axis, or a resting body keeps accumulating gravity
        // under the clamp and tunnels the first frame the clamp misses. Only
        // self's velocity: `other` is typically static terrain that never
        // integrates anyway.
        if (correction.x != 0.0f) velocity.x = 0.0f;
        if (correction.y != 0.0f) velocity.y = 0.0f;

        // correction.y < 0 means this body was pushed up, i.e. `other` is below it.
        if (correction.y < 0.0f) m_Grounded = true;

        return true;
    }

    // Applies a correction computed elsewhere -- by a collider that isn't a pair
    // of AABBs, namely TerrainChunk::ResolveBody -- so the heightmap path keeps
    // the same move/zero-velocity/grounded behaviour as everything above.
    //
    // Not shared with ResolveCollisionWith on purpose: that one splits the
    // correction by mass but decides velocity and grounding from the FULL
    // correction, so routing it through here would change what an immovable body
    // sees when resolving against a movable one.
    void ApplyCollisionCorrection(const Vector2& correction) {
        if (correction.x == 0.0f && correction.y == 0.0f) return;

        transform.position += correction;
        if (correction.x != 0.0f) velocity.x = 0.0f;
        if (correction.y != 0.0f) velocity.y = 0.0f;
        if (correction.y < 0.0f) m_Grounded = true;
    }

    // Clamps this body inside [0,0]..[windowWidth,windowHeight]. An absolute
    // boundary, not a pushable collision, so it ignores mass entirely. Uses the
    // shape's world AABB when one is attached, else a box around `size`.
    // Returns true if a clamp happened.
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
        if (correction.y < 0.0f) m_Grounded = true;

        return true;
    }

private:
    Vector2 m_ForceAccum;
    bool m_Grounded = false;
    inline static Vector2 s_Gravity = Vector2::Zero();
};
