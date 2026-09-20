#pragma once

class IGraphicsContext;
class IWindow;
class Renderer2D;
class ActorRegistry;
class UserInputService;
class LightingSystem;
class TerrainSystem;

// Non-owning bundle of the engine subsystems exposed to Lua.
struct EngineContext {
    IGraphicsContext* graphics = nullptr;
    IWindow* window = nullptr;
    Renderer2D* renderer = nullptr;
    ActorRegistry* actors = nullptr;
    UserInputService* input = nullptr;
    LightingSystem* lighting = nullptr;
    TerrainSystem* terrain = nullptr;
};
