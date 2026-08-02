#include "ActorRegistry.h"
#include "Physics/RigidBody2D.h"
#include "Math/Transform2D.h"
#include "Renderer/Shader.h"
#include "Renderer/BuiltInShaders.h"

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