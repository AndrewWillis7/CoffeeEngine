#pragma once
#include "IGraphicsContext.h"

// Forward declarations instead of including windows.h -- same discipline as
// WindowsWindow.h / GLXGraphicsContext.h. With STRICT (the MinGW/MSVC
// default) HWND/HDC/HGLRC are pointers to these incomplete structs.
struct HWND__;
struct HDC__;
struct HGLRC__;

class WGLGraphicsContext : public IGraphicsContext {
public:
    WGLGraphicsContext(void* instance, void* window);
    ~WGLGraphicsContext() override;

    void Init() override;
    void SwapBuffers() override;
    void SetClearColor(float r, float g, float b) override;
    void DrawDebugQuad(const Transform2D& transform, const Vector2& size, const Color& color) override;

private:
    HWND__* m_Hwnd;
    HDC__* m_Dc = nullptr;
    HGLRC__* m_Context = nullptr;
    int m_Width = 0;
    int m_Height = 0;
};