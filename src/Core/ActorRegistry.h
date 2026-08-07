#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

class RigidBody2D;
class Shader;
class CollisionShape2D;
class PlayerActorConfig;
class PixelSprite;

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
    Shader* GetOrCreateNamedShader(const std::string& name);

    // Loads (or returns the already-loaded) PixelSprite for a given PNG
    // path, keyed by that path string -- same "load once, cache by key"
    // shape as GetOrCreateNamedShader. Returns nullptr if the PNG failed
    // to load (see PixelSprite's own constructor for the warning).
    PixelSprite* GetOrLoadPixelSprite(const std::string& filepath);

    CollisionShape2D* CreateBoxCollisionShape(float halfWidth, float halfHeight, float offsetX = 0.0f, float offsetY = 0.0f);
    CollisionShape2D* CreateCircleCollisionShape(float radius, float offsetX = 0.0f, float offsetY = 0.0f);

    PlayerActorConfig* CreatePlayerConfig();
    size_t GetBodyCount() const {return m_Bodies.size();}

    // Scans all bodies for one with a PlayerActorConfig attached. Returns
    // nullptr if no body has been marked as the player yet. O(n) over
    // bodies -- fine at engine scale, revisit if actor counts get large.
    RigidBody2D* GetPlayerActor() const;

    // Logs every RigidBody2D and what's attached to it (Shader,
    // CollisionShape, PlayerActorConfig) to stdout. Stand-in for a real
    // tree view/inspector.
    void DumpTree() const;

    std::vector<std::string> GetDebugLines() const;

    void Clear();
private:
    std::vector<std::unique_ptr<RigidBody2D>> m_Bodies;
    std::vector<std::unique_ptr<CollisionShape2D>> m_CollisionShapes;
    std::vector<std::unique_ptr<PlayerActorConfig>> m_PlayerConfigs;
    std::vector<std::unique_ptr<Shader>> m_Shaders;
    std::unordered_map<std::string, std::unique_ptr<Shader>> m_NamedShaders;

    // Same "not cleared by Clear()" convention as m_NamedShaders -- these
    // are expensive-to-decode engine assets, not Lua-ephemeral gameplay
    // instances, so a script hot-reload shouldn't force every destructible
    // sprite to reload (and lose whatever's already been punched out of it).
    std::unordered_map<std::string, std::unique_ptr<PixelSprite>> m_PixelSprites;
};