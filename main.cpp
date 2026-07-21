#include "IWindow.h"
#include "IGraphicsContext.h"
#include "Core/ScriptEngine.h"
#include <chrono>
#include <iostream>

// Include the basic GL headers to test a basic screen in main for now... (Remove later)
#include <GL/gl.h>

// Temp Bindings
void SetClearColor(float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
}

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
    })

    // Script Engine
    ScriptEngine scriptEngine;
    sol::state& lua = scriptEngine.GetState();

    // Example to expose window class to Lua
    lua.new_usertype<IWindow>("Window",
        "ShouldClose", &IWindow::ShouldClose,
        "GetWidth", &IWindow::GetWidth,
        "GetHeight", &IWindow::GetHeight
    );

    lua["engineWindow"] = window.get();
    lua.set_function("SetClearColor", &SetClearColor);

    scriptEngine.LoadScript("scripts/main.lua");
    scriptEngine.CallFunction("Init");

    auto lastTime = std::chrono::high_resolution_clock::now();

    // CORE LOOP (For now)
    while (!window->ShouldClose()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaDuration = currentTime - lastTime;
        float deltaTime = deltaDuration.count();
        window->PollEvents();

        scriptEngine.CallFunction("Update", deltaTime);

        // Swap the front and back buffers
        glClear(GL_COLOR_BUFFER_BIT);
        graphicsContext->SwapBuffers();
    }

    std::cout << "Engine shut down cleanly." << std::endl;
    return 0;
}