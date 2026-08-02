#pragma once

class IGraphicsContext;
class IWindow;

// Bundles pointers to engine subsystems that need exposing

struct EngineContext {
    IGraphicsContext* graphics = nullptr;
    IWindow* window = nullptr;
};