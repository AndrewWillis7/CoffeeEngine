#pragma once
#include "Core/Math/Transform2D.h"
#include "Core/Math/Color.h"
#include <memory>

class Shader;

// The Real Drawing Pipeline, Shader Based, VBO backend.
// Deliberately knows nothing on the OS side

// THIS DOES ASSUME A GL CONTEXT IS CREATED!!!!

class Renderer2D {
public:
    Renderer2D();
    ~Renderer2D();

    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    // Call once after the GL context is current
    void Init();
    void Shutdown();

    // Call at startup and whenever the window resizes
    void SetViewportSize(int width, int height);

    // Call once per frame before any Draw calls
    void BeginFrame(float deltaTime);
    
    // Basic Quad Draw at Transform, can have applied shaders
    void DrawQuad(const Transform2D& transform, const Vector2& size, const Color& color, Shader* shader);

    Shader* GetDefaultShader() const { return m_DefaultShader.get();}

private:
    void ApplyCommonUniforms(Shader& shader, const Transform2D& transform, const Vector2& size, const Color& color) const;

    unsigned int m_VBO = 0;
    std::unique_ptr<Shader> m_DefaultShader;

    float m_Width = 1.0f;
    float m_Height = 1.0f;
    float m_Time = 0.0f;

    bool m_Initialized = false;
};