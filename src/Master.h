#pragma once

#include <windows.h>
#include <memory>
#include <vector>

#include "Window.h"
#include "Actor.h"
#include "AssetManager.h"

class Master {
public:
    Master(HINSTANCE hInstance);
    ~Master();

    bool Initialize();
    void Run();

    // Inputs
    void OnMouseClick(int x, int y);

private:
    HINSTANCE m_hInstance;
    bool m_isRunning;

    std::unique_ptr<Window> m_window;
    std::vector<std::unique_ptr<Actor>> m_actors;
    AssetManager m_assets;
};