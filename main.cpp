#include "IWindow.h"
#include "IGraphicsContext.h"
#include "Core/ScriptEngine.h"
#include "Core/EngineContext.h"
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

    window->SetEventCallback([](const WindowEvent& e){
        if (e.type == WindowEvent::Type::Close) {
            std::cout << "Event: Window Closed!" << std::endl;
        }
    });

    // Script Stuff
    EngineContext engineContext;
    engineContext.graphics = graphicsContext.get();
    engineContext.window = window.get();

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
        scriptEngine.Update(deltaTime);

        glClear(GL_COLOR_BUFFER_BIT);
        graphicsContext->SwapBuffers();
    }

    std::cout << "Engine shut down cleanly." << std::endl;
    return 0;
}