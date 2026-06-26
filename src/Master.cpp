#include "Master.h"
#include "../src/components/Pet.h"
#include "../src/components/Container.h"
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
ULONG_PTR gdiToken;

Master::Master(HINSTANCE hInstance) :
    m_hInstance(hInstance), m_isRunning(false) {}

Master::~Master() {
    GdiplusShutdown(gdiToken);
}

bool Master::Initialize() {
    GdiplusStartupInput gdiStartupInput;
    if (GdiplusStartup(&gdiToken, &gdiStartupInput, NULL) != Ok) {
        MessageBoxW(NULL, L"GDI+ Initialization Failed", L"Error", MB_OK);
        return false;
    }

    // Independant window (no actor dependancies)
    RECT workingArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workingArea, 0);

    int width = workingArea.right - workingArea.left;
    int height = workingArea.bottom - workingArea.top;

    m_window = std::make_unique<Window>(m_hInstance, width, height);

    if (!m_window->Create(L"Desktop Pet", 0, 0)) {
        return false;
    }

    // Load intro Assets (Maybe move to a game manager later?) (Really need to move this to something dynamic...)
    m_assets.LoadSprite(L"idle", L"./Art/seal_still.png", 32, 32, 1, 1);
    m_assets.LoadSprite(L"walk", L"./Art/walking_sheet.png", 32, 32, 2, 2);
    m_assets.LoadSprite(L"Crate",L"./Art/crate.png", 32, 32, 2, 2);

    // Create the Ground Plane
    float ground = (float)(height - 100);

    // Create the Pet Start Object
    auto pet = std::make_unique<Pet>();
    pet->SetPosition(200.0f, ground);
    pet->SetScale(4.0f);
    pet->SetSpriteKey(L"Idle");

    // Object creation should be streamlined a lot, I dont want to take 5 lines of code to create an object in the gamespace...
    auto crate = std::make_unique<Container>();
    crate->SetPosition(500.0f, ground);
    crate->SetScale(4.0f);
    crate->SetSpriteKey(L"Crate");
    crate->m_activeAnim(4, 0.15f);

    pet->SetGround(ground);
    crate->SetGround(ground);

    // MAKE IT SO LUA OR SOMETHING HANDLES CREATING SPECIAL OBJECTS, THEN THE C++ END ACTUALLY LOADS THEM!!!
    m_actors.push_back(std::move(pet));
    m_actors.push_back(std::move(crate));

    return true;
}

void Master::Run() {
    m_isRunning = true;

    MSG msg = {};
    DWORD lastTime = GetTickCount();

    while (m_isRunning) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_isRunning = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!m_isRunning) break;

        DWORD currentTime = GetTickCount();
        float dt = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        std::vector<RenderData> renderQueue;

        for (auto& actor : m_actors) {
            actor->update(dt);
            renderQueue.push_back(actor->GetRenderData());
        }

        m_window->Render(renderQueue, m_assets);
        Sleep(16); // ~60FPS
    }
}

// All inputs should kinda handle this system where its dynamic...
void Master::OnMouseClick(int x, int y) {
    for (auto& actor : m_actors) {
        if (auto i = dynamic_cast<Actor*>(actor.get())) {
            i->OnClick(x, y);
        }
    }
}
