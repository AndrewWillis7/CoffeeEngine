#include "IWindow.h"
#include "IGraphicsContext.h"
#include "Core/ScriptEngine.h"
#include "Core/EngineContext.h"
#include "Core/ActorRegistry.h"
#include "Renderer/Renderer2D.h"
#include <chrono>
#include <iostream>

// Include the basic GL headers to test a basic screen in main for now... (Remove later)
#include <GL/gl.h>

int main() {
    std::cout << "Initializeing Engine..." << std::endl;

    // Create Window outside of OS scope
    auto window = IWindow::Create("Engine Test Window", 800, 600);

    // Create and initialize Graphics Context outside of OS scope
    auto graphicsContext = IGraphicsContext::Create(
        window->GetNativeDisplay(),
        window->GetNativeWindow()
    );
    graphicsContext->Init();

    // Shader-based drawing pipeline. Lives outside OS
    // Must be Initialized after graphics context
    Renderer2D renderer2D;
    renderer2D.Init();
    renderer2D.SetViewportSize(window->GetWidth(), window->GetHeight());

    // Owns Rigidbody and shader creation at runtime
    ActorRegistry actorRegistry;

    window->SetEventCallback([&renderer2D](const WindowEvent& e){
        if (e.type == WindowEvent::Type::Close) {
            std::cout << "Event: Window Closed!" << std::endl;
        } else if (e.type == WindowEvent::Type::Resize) {
            renderer2D.SetViewportSize(e.width, e.height);
        }
    });

    // Script Stuff
    EngineContext engineContext;
    engineContext.graphics = graphicsContext.get();
    engineContext.window = window.get();
    engineContext.renderer = &renderer2D;
    engineContext.actors = &actorRegistry;

    ScriptEngine scriptEngine;
    scriptEngine.Init("scripts/main.lua", engineContext);

    // LOOP STUFF

    auto lastTime = std::chrono::high_resolution_clock::now();

    // CORE LOOP (For now)
    while (!window->ShouldClose()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaDuration = currentTime - lastTime;

        float deltaTime = deltaDuration.count();
        lastTime = currentTime;

        window->PollEvents();

        glClear(GL_COLOR_BUFFER_BIT);
        renderer2D.BeginFrame(deltaTime);
        scriptEngine.Update(deltaTime);
        
        graphicsContext->SwapBuffers();
    }

    std::cout << "Engine shut down cleanly." << std::endl;
    return 0;
}