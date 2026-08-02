#pragma once
#include "../Math/Vector2.h"
#include "../Math/Transform2D.h"
#include "../Math/Color.h"

class Shader;

// Minimal liner-motion body: velocity + accumulated force, integrated with
// semi-implicit Euler. No collision response (for now...)
class RigidBody2D {
public:
    Transform2D transform;
    Vector2 size = Vector2(50.0f, 50.0f);
    Color color = Color::White();

    // Non-owninf, Renderer2Ds bult in flat color shader
    // lifetime is owned by ActorRegistry
    Shader* shader = nullptr;

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
private:
    Vector2 m_ForceAccum;
};