#pragma once
#include <memory>

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;

    // Initialize the API and binds it to the window
    virtual void Init() = 0;

    // Pushes the rendered frame to the screen
    virtual void SwapBuffers() = 0;

    // Factory Method (Implemented in a factory cpp file)
    static std::unique_ptr<IGraphicsContext> Create(void* nativeDisplay, void* nativeWindow);
};