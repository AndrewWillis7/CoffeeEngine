#pragma once

class IGraphicsContext;
class IWindow;
class Renderer2D;
class ActorRegistry;
class UserInputService;
class LightingSystem;
class TerrainSystem;

// Bundles pointers to engine subsystems that need exposing

struct EngineContext {
    IGraphicsContext* graphics = nullptr;
    IWindow* window = nullptr;
    Renderer2D* renderer = nullptr;
    ActorRegistry* actors = nullptr;
    UserInputService* input = nullptr;
    LightingSystem* lighting = nullptr;
    TerrainSystem* terrain = nullptr;
};