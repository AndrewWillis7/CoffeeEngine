#pragma once
#include <memory>
#include "Core/Math/Transform2D.h"
#include "Core/Math/Color.h"

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;

    // Initialize the API and binds it to the window
    virtual void Init() = 0;

    // Pushes the rendered frame to the screen
    virtual void SwapBuffers() = 0;

    virtual void SetClearColor(float r, float g, float b) = 0;

    // Draw a solid-color quad in screen space
    virtual void DrawDebugQuad(const Transform2D& transform, const Vector2& size, const Color& color) = 0;

    // Factory Method (Implemented in a factory cpp file)
    static std::unique_ptr<IGraphicsContext> Create(void* nativeDisplay, void* nativeWindow);
};