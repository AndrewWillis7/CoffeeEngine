#include "ActorRegistry.h"
#include "Physics/RigidBody2D.h"
#include "Physics/CollisionShape2D.h"
#include "Gameplay/PlayerActorConfig.h"
#include "Gameplay/Camera2D.h"
#include "Gameplay/LightEmitterConfig.h"
#include "Gameplay/Terrain/TerrainChunk.h"
#include "Gameplay/DebugQuadConfig.h"
#include "Math/Transform2D.h"
#include "Renderer/Shader.h"
#include "Renderer/ShaderLibrary.h"
#include "Renderer/PixelSprite.h"
#include <iostream>

ActorRegistry::ActorRegistry() {
    // Built-in named effects. Add one by dropping GLSL under scripts/shaders/
    // and adding a RegisterFromFile() line here.
    ShaderLibrary::RegisterFromFile("Glow", "scripts/shaders/glow.frag");
    ShaderLibrary::RegisterFromFile("RoundedPanel", "scripts/shaders/rounded_panel.frag");
    ShaderLibrary::RegisterFromFile("Textured", "scripts/shaders/textured.frag");
    ShaderLibrary::RegisterFromFile("Text", "scripts/shaders/text.frag");
    ShaderLibrary::RegisterFromFile("Border", "scripts/shaders/border.frag");

    // Seed "Border" so a script that never touches it still gets a night sky
    // instead of the flat black that zero-initialised uniforms would give.
    // u_PixelScale is deliberately omitted: SetActiveCamera sets it every frame.
    Shader* border = GetOrCreateNamedShader("Border");
    if (border && border->IsValid()) {
        border->SetVec3("u_SkyColor", 0.02f, 0.02f, 0.035f);
        border->SetVec3("u_StarColor", 0.95f, 0.95f, 1.0f);
        border->SetVec3("u_CloudColor", 0.12f, 0.12f, 0.14f);
        border->SetFloat("u_StarCellSize", 8.0f);
        border->SetFloat("u_CloudCellSize", 48.0f);
        border->SetFloat("u_CloudBlockSize", 3.0f);
        border->SetFloat("u_CloudSpeed", 6.0f);
        border->SetFloat("u_StarTwinkleSpeed", 1.5f);
    }
}

ActorRegistry::~ActorRegistry() = default;

RigidBody2D* ActorRegistry::CreateRigidBody(float x, float y, float w, float h) {
    auto body = std::make_unique<RigidBody2D>();
    body->transform.position = Vector2(x, y);
    body->size = Vector2(w, h);
    RigidBody2D* raw = body.get();
    m_Bodies.push_back(std::move(body));
    return raw;
}

Shader* ActorRegistry::CreateShader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    auto shader = std::make_unique<Shader>(vertexSrc, fragmentSrc);
    Shader* raw = shader.get();
    m_Shaders.push_back(std::move(shader));
    return raw;
}

Shader* ActorRegistry::CreateGlowShader() {
    // Reuses the registered "Glow" source rather than re-reading the file: the
    // caller wants a separate instance with its own uniform state.
    const ShaderLibrary::Entry* entry = ShaderLibrary::Find("Glow");
    Shader* shader = entry
        ? CreateShader(entry->vertexSrc, entry->fragmentSrc)
        : CreateShader(ShaderLibrary::SharedVertexSrc(), "");

    if (shader->IsValid()) {
        shader->overdrawScale = 1.8f;
        shader->SetVec3("u_GlowColor", 1.0f, 1.0f, 1.0f);
        shader->SetFloat("u_GlowIntensity", 1.2f);
    } else if (!entry) {
        std::cerr << "Engine Warning: CreateGlowShader() couldn't find the 'Glow' shader "
                     "(scripts/shaders/glow.frag failed to load earlier) -- returning an invalid shader.\n";
    }
    return shader;
}

Shader* ActorRegistry::GetOrCreateNamedShader(const std::string& name) {
    auto it = m_NamedShaders.find(name);
    if (it != m_NamedShaders.end()) {
        return it->second.get();
    }

    const ShaderLibrary::Entry* entry = ShaderLibrary::Find(name);
    if (!entry) {
        std::cerr << "Engine Warning: no shader registered under name '" << name << "'\n";
        return nullptr;
    }

    auto shader = std::make_unique<Shader>(entry->vertexSrc, entry->fragmentSrc);
    Shader* raw = shader.get();
    m_NamedShaders.emplace(name, std::move(shader));
    return raw;
}

bool ActorRegistry::LoadNamedShaderFromFile(const std::string& name, const std::string& fragmentPath) {
    std::string fragmentSrc;
    if (!ShaderLibrary::ReadFile(fragmentPath, fragmentSrc)) {
        std::cerr << "Engine Warning: couldn't open '" << fragmentPath << "' for shader '" << name << "'\n";
        return false;
    }

    auto shader = std::make_unique<Shader>(ShaderLibrary::SharedVertexSrc(), fragmentSrc);
    if (!shader->IsValid()) {
        // Shader's constructor already printed the GLSL error; name the slot it
        // came from, then leave the previously cached shader alone.
        std::cerr << "Engine Warning: shader '" << name << "' from '" << fragmentPath
                   << "' failed to compile -- keeping the previous shader (if any).\n";
        return false;
    }

    // Keep the library in sync so a later uncached lookup sees this source too.
    ShaderLibrary::Register(name, fragmentSrc);

    m_NamedShaders.insert_or_assign(name, std::move(shader));
    return true;
}

CollisionShape2D* ActorRegistry::CreateBoxCollisionShape(float halfWidth, float halfHeight, float offsetX, float offsetY) {
    auto shape = std::make_unique<CollisionShape2D>(
        CollisionShape2D::MakeBox(Vector2(halfWidth, halfHeight), Vector2(offsetX, offsetY)));
    CollisionShape2D* raw = shape.get();
    m_CollisionShapes.push_back(std::move(shape));
    return raw;
}

CollisionShape2D* ActorRegistry::CreateCircleCollisionShape(float radius, float offsetX, float offsetY) {
    auto shape = std::make_unique<CollisionShape2D>(
        CollisionShape2D::MakeCircle(radius, Vector2(offsetX, offsetY)));
    CollisionShape2D* raw = shape.get();
    m_CollisionShapes.push_back(std::move(shape));
    return raw;
}

PlayerActorConfig* ActorRegistry::CreatePlayerConfig() {
    auto config = std::make_unique<PlayerActorConfig>();
    PlayerActorConfig* raw = config.get();
    m_PlayerConfigs.push_back(std::move(config));
    return raw;
}

RigidBody2D* ActorRegistry::GetPlayerActor() const {
    for (const auto& body : m_Bodies) {
        if (body->playerConfig) return body.get();
    }
    return nullptr;
}

Camera2D* ActorRegistry::CreateCamera() {
    auto camera = std::make_unique<Camera2D>();
    Camera2D* raw = camera.get();
    m_Cameras.push_back(std::move(camera));
    return raw;
}

RigidBody2D* ActorRegistry::GetActiveCamera() const {
    for (const auto& body : m_Bodies) {
        if (body->camera && body->camera->active) return body.get();
    }
    return nullptr;
}

PixelSprite* ActorRegistry::GetOrLoadPixelSprite(const std::string& filepath) {
    auto it = m_PixelSprites.find(filepath);
    if (it != m_PixelSprites.end()) return it->second.get();

    auto sprite = std::make_unique<PixelSprite>(filepath);
    if (!sprite->IsValid()) return nullptr; // PixelSprite's constructor logged why

    PixelSprite* raw = sprite.get();
    m_PixelSprites.emplace(filepath, std::move(sprite));
    return raw;
}

PixelSprite* ActorRegistry::CreateSolidSprite(int width, int height, float r, float g, float b, float a) {
    auto sprite = std::make_unique<PixelSprite>(width, height, Color(r, g, b, a));
    if (!sprite->IsValid()) return nullptr; // PixelSprite's constructor logged why

    PixelSprite* raw = sprite.get();
    m_GeneratedSprites.push_back(std::move(sprite));
    return raw;
}

LightEmitterConfig* ActorRegistry::CreateLightEmitter() {
    auto config = std::make_unique<LightEmitterConfig>();
    LightEmitterConfig* raw = config.get();
    m_LightEmitters.push_back(std::move(config));
    return raw;
}

TerrainChunk* ActorRegistry::CreateTerrainChunk() {
    auto chunk = std::make_unique<TerrainChunk>();
    TerrainChunk* raw = chunk.get();
    m_TerrainChunks.push_back(std::move(chunk));
    return raw;
}

RigidBody2D* ActorRegistry::CreateDebugQuad(float x, float y) {
    auto config = std::make_unique<DebugQuadConfig>();
    DebugQuadConfig* rawConfig = config.get();
    m_DebugQuads.push_back(std::move(config));

    const float size = static_cast<float>(DebugQuadConfig::kCellSize);
    RigidBody2D* body = CreateRigidBody(x, y, size, size);
    body->debugQuad = rawConfig;
    body->sprite = CreateSolidSprite(DebugQuadConfig::kCellSize, DebugQuadConfig::kCellSize, 0.13f, 0.13f, 0.16f, 1.0f);
    if (body->sprite) GenerateDebugQuadTexture(*body->sprite, rawConfig->cellsX, rawConfig->cellsY);
    return body;
}

void ActorRegistry::DumpTree() const {
    std::cout << "ActorRegistry (" << m_Bodies.size() << " bodies)\n";
    for (const auto& body : m_Bodies) {
        std::cout << "  RigidBody2D @ (" << body->transform.position.x << ", "
                   << body->transform.position.y << ")\n";
        std::cout << "    - PhysicsObject: mass=" << body->mass
                   << " drag=" << body->drag
                   << " velocity=(" << body->velocity.x << ", " << body->velocity.y << ")\n";
        std::cout << "    - Shader: " << (body->shader ? "attached" : "none") << "\n";
        if (body->collisionShape) {
            const char* shapeType = body->collisionShape->GetType() == CollisionShape2D::Type::Box ? "Box" : "Circle";
            std::cout << "    - CollisionShapeObject: " << shapeType << "\n";
        } else {
            std::cout << "    - CollisionShapeObject: none\n";
        }
        if (body->playerConfig) {
            std::cout << "    - PlayerActorConfig: attached (moveSpeed=" << body->playerConfig->moveSpeed << ")\n";
        } else {
            std::cout << "    - PlayerActorConfig: none\n";
        }
        if (body->camera) {
            std::cout << "    - Camera2D: attached (active=" << (body->camera->active ? "true" : "false")
                       << " viewport=" << body->camera->viewportSize.x << "x" << body->camera->viewportSize.y
                       << " zoomOut=" << body->camera->GetZoomOut() << ")\n";
        } else {
            std::cout << "    - Camera2D: none\n";
        }
        if (body->lightEmitter) {
            std::cout << "    - LightEmitterConfig: attached (radius=" << body->lightEmitter->radius
                       << " brightness=" << body->lightEmitter->brightness << ")\n";
        } else {
            std::cout << "    - LightEmitterConfig: none\n";
        }
        std::cout << "    - lightBlocking: " << (body->lightBlocking ? "true" : "false") << "\n";
    }
}

size_t ActorRegistry::GetSpritePixelCount() const {
    size_t total = 0;
    for (const auto& entry : m_PixelSprites) {
        total += static_cast<size_t>(entry.second->GetWidth()) * entry.second->GetHeight();
    }
    for (const auto& sprite : m_GeneratedSprites) {
        total += static_cast<size_t>(sprite->GetWidth()) * sprite->GetHeight();
    }
    return total;
}

void ActorRegistry::Clear() {
    m_Bodies.clear();
    m_Shaders.clear();
    m_CollisionShapes.clear();
    m_PlayerConfigs.clear();
    m_Cameras.clear();
    m_LightEmitters.clear();
    m_TerrainChunks.clear();
    m_DebugQuads.clear();
    m_GeneratedSprites.clear();

    // Non-owning and may point into m_GeneratedSprites, just freed above. There is
    // no cheap way to tell which pool it came from, so always drop it and let the
    // reloaded script's Init() set it again.
    m_BorderSprite = nullptr;
}