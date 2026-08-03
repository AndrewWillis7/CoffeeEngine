#include "ActorRegistry.h"
#include "Physics/RigidBody2D.h"
#include "Physics/CollisionShape2D.h"
#include "Gameplay/PlayerActorConfig.h"
#include "Math/Transform2D.h"
#include "Renderer/Shader.h"
#include "Renderer/BuiltInShaders.h"
#include <iostream>

ActorRegistry::ActorRegistry() = default;
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
        shader->overdrawScale = 1.8f; // Let the halo bleed past the bodys own size a bit
        shader->SetVec3("u_GlowColor", 1.0f, 1.0f, 1.0f);
        shader->SetFloat("u_GlowIntensity", 1.2f);
    }
    return shader;
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