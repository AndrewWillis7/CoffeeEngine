#pragma once
#include <memory>
#include <string>
#include <vector>

class RigidBody2D;
class Shader;
class CollisionShape2D;
class PlayerActorConfig;

// Owns the Rigidbody2D, Shader, CollisionShape2D, and PlayerActorConfig
// instances created from LUA. Outlives the individual script calls but
// still gets cleaned up after shutdown.
// Mind the order of creation on main.cpp to gurantee GL context destruction
class ActorRegistry {
public:
    // Declared and not defaulted here. Defined in the CPP
    ActorRegistry();
    ~ActorRegistry();

    RigidBody2D* CreateRigidBody(float x, float y, float w, float h);
    Shader* CreateShader(const std::string& vertexSrc, const std::string& fragmentSrc);
    Shader* CreateGlowShader();

    CollisionShape2D* CreateBoxCollisionShape(float halfWidth, float halfHeight, float offsetX = 0.0f, float offsetY = 0.0f);
    CollisionShape2D* CreateCircleCollisionShape(float radius, float offsetX = 0.0f, float offsetY = 0.0f);

    PlayerActorConfig* CreatePlayerConfig();

    // Scans all bodies for one with a PlayerActorConfig attached. Returns
    // nullptr if no body has been marked as the player yet. O(n) over
    // bodies -- fine at engine scale, revisit if actor counts get large.
    RigidBody2D* GetPlayerActor() const;

    // Logs every RigidBody2D and what's attached to it (Shader,
    // CollisionShape, PlayerActorConfig) to stdout. Stand-in for a real
    // tree view/inspector.
    void DumpTree() const;
private:
    std::vector<std::unique_ptr<RigidBody2D>> m_Bodies;
    std::vector<std::unique_ptr<Shader>> m_Shaders;
    std::vector<std::unique_ptr<CollisionShape2D>> m_CollisionShapes;
    std::vector<std::unique_ptr<PlayerActorConfig>> m_PlayerConfigs;
};