#include "ActorRegistry.h"
#include "Physics/RigidBody2D.h"
#include "Physics/CollisionShape2D.h"
#include "Gameplay/PlayerActorConfig.h"
#include "Math/Transform2D.h"
#include "Renderer/Shader.h"
#include "Renderer/BuiltInShaders.h"
#include "Renderer/ShaderLibrary.h"
#include <sstream>
#include <iostream>

ActorRegistry::ActorRegistry() {
    // Register the engine's built-in named effects once. Add a new one by
    // writing the GLSL and adding one Register() line here.
    ShaderLibrary::Register("Glow", BuiltInShaders::GlowFragmentSrc);
    ShaderLibrary::Register("RoundedPanel", BuiltInShaders::RoundedPanelFragmentSrc);
    ShaderLibrary::Register("Textured", BuiltInShaders::TexturedFragmentSrc);
    ShaderLibrary::Register("Text", BuiltInShaders::TextFragmentSrc);
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
    Shader* shader = CreateShader(BuiltInShaders::QuadVertexSrc, BuiltInShaders::GlowFragmentSrc);
    if (shader->IsValid()) {
        shader->overdrawScale = 1.8f;
        shader->SetVec3("u_GlowColor", 1.0f, 1.0f, 1.0f);
        shader->SetFloat("u_GlowIntensity", 1.2f);
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
    m_NamedShaders[name] = std::move(shader);
    return raw;
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
    }
}

std::vector<std::string> ActorRegistry::GetDebugLines() const {
    std::vector<std::string> lines;
    lines.push_back(std::to_string(m_Bodies.size()) + " bodies");

    for (size_t i = 0; i < m_Bodies.size(); ++i) {
        const auto& body = m_Bodies[i];
        std::ostringstream line;
        line << "[" << i << "] (" << static_cast<int>(body->transform.position.x)
             << "," << static_cast<int>(body->transform.position.y) << ")";
        lines.push_back(line.str());

        std::ostringstream flags;
        flags << "  " << (body->shader ? "[Shd]" : "") 
              << (body->collisionShape ? (body->collisionShape->GetType() == CollisionShape2D::Type::Box ? "[Box]" : "[Circ]") : "")
              << (body->playerConfig ? "[Ply]" : "");
        lines.push_back(flags.str());
    }

    if (m_Bodies.empty()) {
        lines.push_back("(no actors loaded)");
    }
    return lines;
}

void ActorRegistry::Clear() {
    m_Bodies.clear();
    m_Shaders.clear();
    m_CollisionShapes.clear();
    m_PlayerConfigs.clear();
    // m_NamedShaders intentionally NOT cleared -- see header comment.
}