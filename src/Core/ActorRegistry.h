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
class Camera2D;
class LightEmitterConfig;
class TerrainChunk;
class DebugQuadConfig;

// Sole owner of every engine-side object Lua can create at runtime. Lua holds
// raw pointers into these pools, never memory of its own.
// Construction order in main.cpp matters: this must die before the GL context.
class ActorRegistry {
public:
    ActorRegistry();
    ~ActorRegistry();

    RigidBody2D* CreateRigidBody(float x, float y, float w, float h);
    Shader* CreateShader(const std::string& vertexSrc, const std::string& fragmentSrc);
    Shader* CreateGlowShader();
    Shader* GetOrCreateNamedShader(const std::string& name);

    // Compiles a .frag off disk against the shared quad vertex shader and
    // REPLACES whatever is cached under `name` (GetOrCreateNamedShader only ever
    // creates on first use). On a missing file or compile error, warns and leaves
    // the previous shader intact. Returns true on success.
    bool LoadNamedShaderFromFile(const std::string& name, const std::string& fragmentPath);

    // Loads a PNG once and caches it by path.
    PixelSprite* GetOrLoadPixelSprite(const std::string& filepath);

    // A blank sprite filled with (r,g,b,a). Deliberately not cached: every caller
    // needs its own pixel buffer, since these get punched/destructed in place.
    PixelSprite* CreateSolidSprite(int width, int height, float r, float g, float b, float a);

    CollisionShape2D* CreateBoxCollisionShape(float halfWidth, float halfHeight, float offsetX = 0.0f, float offsetY = 0.0f);
    CollisionShape2D* CreateCircleCollisionShape(float radius, float offsetX = 0.0f, float offsetY = 0.0f);

    PlayerActorConfig* CreatePlayerConfig();
    size_t GetBodyCount() const {return m_Bodies.size();}

    Camera2D* CreateCamera();
    LightEmitterConfig* CreateLightEmitter();
    TerrainChunk* CreateTerrainChunk();

    // A placed "debug quad" asset (see DebugQuadConfig): a 1x1-cell body with a
    // procedurally generated grid texture, ready to be named and edited by the
    // scene editor. Exposed to Lua as Actors.CreateDebugQuad so the per-frame
    // DrawBody() a spawned asset needs can come from the Lua-side registry that
    // calls it, the same as any other visible object.
    RigidBody2D* CreateDebugQuad(float x, float y);

    // Non-owning; the sprite belongs to whichever pool created it. Drawn behind
    // the letterbox margins when the active "Border" shader wants a texture.
    void SetBorderSprite(PixelSprite* sprite) { m_BorderSprite = sprite; }
    PixelSprite* GetBorderSprite() const { return m_BorderSprite; }

    // First body with a PlayerActorConfig / an active Camera2D, or nullptr.
    // O(n) over bodies -- fine at engine scale, revisit alongside a broad phase.
    RigidBody2D* GetPlayerActor() const;
    RigidBody2D* GetActiveCamera() const;

    const std::vector<std::unique_ptr<RigidBody2D>>& GetBodies() const { return m_Bodies; }

    // Pool sizes, for the debug panel. Named shaders and disk-loaded sprites
    // are counted with the rest even though Clear() spares them, because what
    // this answers is "how much is loaded", not "how much is ephemeral".
    size_t GetCollisionShapeCount() const { return m_CollisionShapes.size(); }
    size_t GetCameraCount() const { return m_Cameras.size(); }
    size_t GetLightEmitterCount() const { return m_LightEmitters.size(); }
    size_t GetTerrainChunkCount() const { return m_TerrainChunks.size(); }
    size_t GetDebugQuadCount() const { return m_DebugQuads.size(); }
    size_t GetShaderCount() const { return m_Shaders.size() + m_NamedShaders.size(); }
    size_t GetSpriteCount() const { return m_PixelSprites.size() + m_GeneratedSprites.size(); }

    // Total texels across every sprite in both pools -- a stand-in for how
    // much pixel memory the scene is holding. Out of line: PixelSprite is
    // only forward declared here.
    size_t GetSpritePixelCount() const;

    void DumpTree() const;

    void Clear();
private:
    std::vector<std::unique_ptr<RigidBody2D>> m_Bodies;
    std::vector<std::unique_ptr<CollisionShape2D>> m_CollisionShapes;
    std::vector<std::unique_ptr<PlayerActorConfig>> m_PlayerConfigs;
    std::vector<std::unique_ptr<Camera2D>> m_Cameras;
    std::vector<std::unique_ptr<LightEmitterConfig>> m_LightEmitters;
    std::vector<std::unique_ptr<TerrainChunk>> m_TerrainChunks;
    std::vector<std::unique_ptr<DebugQuadConfig>> m_DebugQuads;
    std::vector<std::unique_ptr<Shader>> m_Shaders;

    // Not swept by Clear(): expensive-to-build engine assets (compiled GLSL,
    // decoded PNGs) that a script hot-reload shouldn't have to pay for again.
    std::unordered_map<std::string, std::unique_ptr<Shader>> m_NamedShaders;
    std::unordered_map<std::string, std::unique_ptr<PixelSprite>> m_PixelSprites;

    // Procedurally generated sprites. Cheap to rebuild, so Clear() does sweep these.
    std::vector<std::unique_ptr<PixelSprite>> m_GeneratedSprites;

    PixelSprite* m_BorderSprite = nullptr;
};
