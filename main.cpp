#include "IWindow.h"
#include "IGraphicsContext.h"
#include "Core/ScriptEngine.h"
#include "Core/EngineContext.h"
#include "Core/ActorRegistry.h"
#include "Core/Input/UserInputService.h"
#include "Core/Gameplay/UI/EngineUIController.h"
#include "Core/Gameplay/LightingSystem.h"
#include "Core/Gameplay/Terrain/TerrainSystem.h"
#include "Renderer/Renderer2D.h"
#include <chrono>
#include <iostream>

#include <GL/gl.h>

int main() {
    std::cout << "Initializing Engine..." << std::endl;

    auto window = IWindow::Create("Engine Test Window", 800, 600);

    auto graphicsContext = IGraphicsContext::Create(
        window->GetNativeDisplay(),
        window->GetNativeWindow()
    );
    graphicsContext->Init();

    // Must be initialized after the graphics context.
    Renderer2D renderer2D;
    renderer2D.Init();
    renderer2D.SetViewportSize(window->GetWidth(), window->GetHeight());

    ActorRegistry actorRegistry;
    UserInputService inputService;
    LightingSystem lightingSystem;
    TerrainSystem terrainSystem;

    EngineContext engineContext;
    engineContext.graphics = graphicsContext.get();
    engineContext.window = window.get();
    engineContext.renderer = &renderer2D;
    engineContext.actors = &actorRegistry;
    engineContext.input = &inputService;
    engineContext.lighting = &lightingSystem;
    engineContext.terrain = &terrainSystem;

    ScriptEngine scriptEngine;
    scriptEngine.Init("scripts/main.lua", engineContext);

    EngineUIController engineUI(actorRegistry, renderer2D, inputService, scriptEngine);

    window->SetEventCallback([&renderer2D, &inputService](const WindowEvent& e){
        if (e.type == WindowEvent::Type::Close) {
            std::cout << "Event: Window Closed!" << std::endl;
        } else if (e.type == WindowEvent::Type::Resize) {
            renderer2D.SetViewportSize(e.width, e.height);
        }
        inputService.OnWindowEvent(e);
    });

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!window->ShouldClose()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaDuration = currentTime - lastTime;

        float deltaTime = deltaDuration.count();
        lastTime = currentTime;

        window->PollEvents();

        glClear(GL_COLOR_BUFFER_BIT);
        renderer2D.BeginFrame(deltaTime);
        scriptEngine.Update(deltaTime);

        engineUI.Update(window->GetHeight());
        engineUI.Draw();

        graphicsContext->SwapBuffers();

        // Scripts have read this frame's Pressed/Released edges; clear them so
        // next frame's PollEvents() starts fresh.
        inputService.NewFrame();
    }

    std::cout << "Engine shut down cleanly." << std::endl;
    return 0;
}
