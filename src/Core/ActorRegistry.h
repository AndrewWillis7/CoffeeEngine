#pragma once
#include <memory>
#include <string>
#include <vector>

class RigidBody2D;
class Shader;

// Owns the Rigidbody2D and Shader instance created from LUA
// Outlives the individual script calls but still gets cleaned up after shutdown
// Mind the order of creation on main.cpp to gurantee GL context destruction
class ActorRegistry {
public:
    // Declared and not defaulted here. Defined in the CPP
    ActorRegistry();
    ~ActorRegistry();

    RigidBody2D* CreateRigidBody(float x, float y, float w, float h);
    Shader* CreateShader(const std::string& vertexSrc, const std::string& fragmentSrc);
    Shader* CreateGlowShader();
private:
    std::vector<std::unique_ptr<RigidBody2D>> m_Bodies;
    std::vector<std::unique_ptr<Shader>> m_Shaders;
};