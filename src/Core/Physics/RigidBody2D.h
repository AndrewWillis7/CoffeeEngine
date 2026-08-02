#pragma once
#include "../Math/Vector2.h"
#include "../Math/Transform2D.h"

// Minimal liner-motion body: velocity + accumulated force, integrated with
// semi-implicit Euler. No collision response (for now...)
class RigidBody2D {
public:
    Vector2 velocity;
    float mass = 1.0f;
    float drag = 0.0f; // By simple Linear damping, 0 = none

    void AddForce(const Vector2& force) {m_ForceAccum += force;}
    
    // Advances Velocity from Accumulated force, then advances the transorms position
    // This is form the velocity, and called once per step with a fixed frame or DeltaTime
    void Integrate(Transform2D& transform, float dt) {
        if (mass > 0.0f)
            velocity += (m_ForceAccum / mass) * dt;
        
        if (drag > 0.0f)
            velocity *= (1.0f - drag * dt);

        transform.position += velocity * dt;
        m_ForceAccum = Vector2::Zero();
    }
private:
    Vector2 m_ForceAccum;
};