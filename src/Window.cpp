#include "Window.h"
#include <iostream>

#define TRANSPARENT_COLOR RGB(255, 0, 255)

Window::Window(HINSTANCE hInstance, int width, int height)
    : m_hwnd(nullptr), m_hInstance(hInstance), m_width(width), m_height(height), m_interactive(false) {}

Window::~Window() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);

    CleanupBackBuffer();

    UnregisterClassW(L"DesktopPetWindowClass", m_hInstance);
}

bool Window::Create(const wchar_t* title, int x, int y) {
    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Window::RouteMessage;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = L"DesktopPetWindowClass";
    wc.hbrBackground = CreateSolidBrush(TRANSPARENT_COLOR);

    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"Failed to register class!", L"Error", MB_OK);
        return false;
    }

    //  WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"DesktopPetWindowClass", title, WS_POPUP,
        x, y, m_width, m_height,
        NULL, NULL, m_hInstance, this
    );

    if (!m_hwnd) {
        DWORD err = GetLastError();

        wchar_t buffer[256];
        swprintf_s(buffer, L"CreateWindowEx FAILED\nError code: %lu", err);

        MessageBoxW(NULL, buffer, L"Error", MB_OK);
        return false;
    } else {
        //MessageBoxW(NULL, L"Window Created!", L"Debug", MB_OK);
    }

    SetLayeredWindowAttributes(m_hwnd, TRANSPARENT_COLOR, 0, LWA_COLORKEY);
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    AddTrayIcon();

    HDC hdc = GetDC(m_hwnd);
    InitBackBuffer(hdc);
    ReleaseDC(m_hwnd, hdc);

    return true;
}

void Window::Render(const std::vector<RenderData>& dataList, AssetManager& assets) {
    HDC hdc = GetDC(m_hwnd);

    // Clear
    HBRUSH bg = CreateSolidBrush(RGB(255, 0, 255));
    RECT r = {0, 0, m_width, m_height};
    FillRect(m_memDC, &r, bg);
    DeleteObject(bg);

    // Draw all actors
    for (const auto& data : dataList) {
        Sprite* sprite = assets.GetSprite(data.spriteKey);
        if (sprite) {
            sprite->SetTint(1.0f, 0.3f, 0.3f);
            sprite->Draw(
                m_memDC,
                data.x,
                data.y,
                data.frame,
                data.flip,
                data.scale
            );
        }
    }

    BitBlt(hdc, 0, 0, m_width, m_height, m_memDC, 0, 0, SRCCOPY);
    ReleaseDC(m_hwnd, hdc);
}

void Window::Run(std::function<void(float)> tick) {
    MSG msg = {};
    DWORD lastTime = GetTickCount();

    while (true) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return;
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        DWORD current = GetTickCount();
        float dt = (current - lastTime) / 1000.0f;
        lastTime = current;

        tick(dt);

        Sleep(16);
    }
}

void Window::AddTrayIcon() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    wcscpy_s(nid.szTip, L"mini");

    
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        MessageBoxW(NULL, L"Tray icon failed!", L"Error", MB_OK);
    }

    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
}

void Window::UpdatePosition(int x, int y) {
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, m_width, m_height, SWP_NOSIZE | SWP_NOACTIVATE);
}

void Window::ToggleInteraction() {
    m_interactive = !m_interactive;

    LONG exStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);

    if (m_interactive) {
        SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
    } else {
        SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
    }
}

void Window::InitBackBuffer(HDC hdc) {
    m_memDC = CreateCompatibleDC(hdc);
    m_backBuffer = CreateCompatibleBitmap(hdc, m_width, m_height);
    SelectObject(m_memDC, m_backBuffer);
}

void Window::CleanupBackBuffer() {
    if (m_backBuffer) DeleteObject(m_backBuffer);
    if (m_memDC) DeleteDC(m_memDC);
}

// static router that redirects windows callbacks to our actual object instance
LRESULT CALLBACK Window::RouteMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Window* pWindow = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pWindow = reinterpret_cast<Window*>(pCreate->lpCreateParams);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
        return TRUE;
    }

    pWindow = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (pWindow) {
        return pWindow->HandleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


LRESULT Window::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            ValidateRect(m_hwnd, NULL);
            return 0;
        }

        case WM_TRAYICON: {
            // Check if user right-clicked the system tray icon
            if (LOWORD(lp) == WM_RBUTTONUP) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
                AppendMenuW(hMenu, MF_STRING, 2001, L"Play");

                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(m_hwnd); // Prevents the menu from getting stuck
                
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, m_hwnd, NULL);
                PostMessage(m_hwnd, WM_NULL, 0, 0);
                DestroyMenu(hMenu);
            }
            break;
        }

        case WM_COMMAND: {
            switch(LOWORD(wp)) {
                case 2001:
                    ToggleInteraction();
                    break;
                
                case ID_TRAY_EXIT:
                    PostQuitMessage(0);
                    break;
            }
            break;
        }

        case WM_LBUTTONDOWN:
            if (m_interactive) {
                MessageBoxW(NULL, L"You pet the little guy!", L"Pet", MB_OK);
                return 0;
            }

        case WM_ERASEBKGND:
            return 1;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}