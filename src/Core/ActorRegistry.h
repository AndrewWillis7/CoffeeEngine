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

    // Reads a .frag file off disk (paired with the engine's shared
    // QuadVertexSrc, same as every other named shader) and installs it
    // under `name`, REPLACING whatever's currently cached there -- unlike
    // GetOrCreateNamedShader, which only creates on first use and then
    // keeps serving that same instance forever. This is how Lua swaps out
    // e.g. the "Border" shader for a custom one at runtime: Actors.
    // LoadShaderFromFile("Border", "scripts/shaders/my_border.frag").
    // Also re-registers the source in ShaderLibrary so it's consistent
    // for any other lookup path. If the file is missing or fails to
    // compile, logs a warning and leaves whatever was previously cached
    // (if anything) untouched -- a typo in a live-tunable shader path
    // shouldn't take down rendering. Returns true on success.
    bool LoadNamedShaderFromFile(const std::string& name, const std::string& fragmentPath);

    // Loads (or returns the already-loaded) PixelSprite for a given PNG
    // path, keyed by that path string -- same "load once, cache by key"
    // shape as GetOrCreateNamedShader. Returns nullptr if the PNG failed
    // to load (see PixelSprite's own constructor for the warning).
    PixelSprite* GetOrLoadPixelSprite(const std::string& filepath);

    // Builds a brand new, blank PixelSprite filled solid with (r,g,b,a),
    // sized width x height -- the "turn a flat-color quad into individual
    // pixels" primitive (see PixelSprite's (int,int,Color) constructor).
    // Deliberately NOT pooled/cached like GetOrLoadPixelSprite's PNG
    // loader above -- there's no file path to key on, and every caller
    // wants its OWN independent pixel buffer (a wall and a floor built at
    // the same w/h/color must not secretly share one PunchCircle-able
    // sprite). Owned in a separate pool (m_GeneratedSprites below) that
    // Clear() DOES sweep, unlike m_PixelSprites -- there's no expensive
    // decode step worth preserving across a hot-reload here, Init() just
    // rebuilds it fresh. Returns nullptr if width/height aren't positive.
    PixelSprite* CreateSolidSprite(int width, int height, float r, float g, float b, float a);

    CollisionShape2D* CreateBoxCollisionShape(float halfWidth, float halfHeight, float offsetX = 0.0f, float offsetY = 0.0f);
    CollisionShape2D* CreateCircleCollisionShape(float radius, float offsetX = 0.0f, float offsetY = 0.0f);

    PlayerActorConfig* CreatePlayerConfig();
    size_t GetBodyCount() const {return m_Bodies.size();}

    Camera2D* CreateCamera();

    // Same lifetime/pooling convention as CreatePlayerConfig -- owned
    // here, wiped by Clear() (Lua-ephemeral gameplay state, not an
    // expensive engine asset), attach via RigidBody2D::lightEmitter.
    LightEmitterConfig* CreateLightEmitter();

    // Same lifetime/pooling convention as CreateLightEmitter/
    // CreatePlayerConfig -- owned here, wiped by Clear() (Lua-ephemeral
    // gameplay state, not an expensive engine asset), attached via
    // RigidBody2D::terrain. The generated pixels themselves live in the
    // PixelSprite the chunk is told to paint into, which is exactly why
    // this stays cheap to throw away and rebuild on hot-reload: the
    // heightmap and blade list regenerate from `seed` in a millisecond.
    TerrainChunk* CreateTerrainChunk();

    // Non-owning, same lifetime convention as every other cross-reference
    // here -- the PixelSprite itself is owned by whichever GetOrLoadPixelSprite
    // call created it. Renderer2D::SetActiveCamera (via SyncCamera(), see
    // ScriptBindings.cpp) draws this behind the letterbox/pillarbox
    // margins if the active "Border" shader wants a texture (declares
    // `uniform sampler2D u_Texture`) -- nullptr (the default) just means
    // the border shader runs with no texture bound, which is exactly what
    // the built-in procedural sin-wave border wants.
    void SetBorderSprite(PixelSprite* sprite) { m_BorderSprite = sprite; }
    PixelSprite* GetBorderSprite() const { return m_BorderSprite; }

    // Scans all bodies for one with a PlayerActorConfig attached. Returns
    // nullptr if no body has been marked as the player yet. O(n) over
    // bodies -- fine at engine scale, revisit if actor counts get large.
    RigidBody2D* GetPlayerActor() const;

    // Scans all bodies for one with an active Camera2D attached, same
    // "O(n), fine at engine scale" convention as GetPlayerActor() just
    // above. Returns nullptr if no body has an active camera yet -- in
    // that case Renderer2D falls back to its legacy identity mapping
    // (world pixels == screen pixels), so existing scripts that never
    // create a camera keep drawing exactly as before.
    RigidBody2D* GetActiveCamera() const;

    // Raw access to every body -- used by LightingSystem (see its .cpp),
    // which needs to scan for lightEmitter-tagged bodies AND every
    // sprite-backed candidate a light might touch, neither of which fits
    // a single-purpose Get*() accessor the way GetPlayerActor()/
    // GetActiveCamera() do. Same "O(n) scan, fine at engine scale, revisit
    // alongside real broad-phase" convention as those two.
    const std::vector<std::unique_ptr<RigidBody2D>>& GetBodies() const { return m_Bodies; }

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
    std::vector<std::unique_ptr<Camera2D>> m_Cameras;
    std::vector<std::unique_ptr<LightEmitterConfig>> m_LightEmitters;
    std::vector<std::unique_ptr<TerrainChunk>> m_TerrainChunks;
    std::vector<std::unique_ptr<Shader>> m_Shaders;
    std::unordered_map<std::string, std::unique_ptr<Shader>> m_NamedShaders;

    // Same "not cleared by Clear()" convention as m_NamedShaders -- these
    // are expensive-to-decode engine assets, not Lua-ephemeral gameplay
    // instances, so a script hot-reload shouldn't force every destructible
    // sprite to reload (and lose whatever's already been punched out of it).
    std::unordered_map<std::string, std::unique_ptr<PixelSprite>> m_PixelSprites;

    // Procedurally-generated (not loaded from a path) sprites -- see
    // CreateSolidSprite() above. Unlike m_PixelSprites, these ARE cleared
    // by Clear(): nothing here was expensive to build, so a hot-reload
    // just regenerates them fresh instead of preserving stale state.
    std::vector<std::unique_ptr<PixelSprite>> m_GeneratedSprites;

    // Non-owning -- see SetBorderSprite() above. Deliberately NOT cleared
    // by Clear(): a hot-reload's fresh Init() will just set it again if
    // the script wants a border sprite, same reasoning as m_NamedShaders/
    // m_PixelSprites already not resetting on reload.
    PixelSprite* m_BorderSprite = nullptr;
};