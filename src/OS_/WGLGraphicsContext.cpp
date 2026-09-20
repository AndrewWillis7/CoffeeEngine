#include "WGLGraphicsContext.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <iostream>
#include <cstdlib>

// Mirrors GLXGraphicsContext: a legacy (compatibility) context from
// wglCreateContext, which still exposes GL 2.0+ entrypoints through
// wglGetProcAddress -- that's all GLLoader needs for GLSL 120.

WGLGraphicsContext::WGLGraphicsContext(void* /*instance*/, void* window)
    : m_Hwnd(static_cast<HWND>(window)) {}

WGLGraphicsContext::~WGLGraphicsContext() {
    if (m_Context) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_Context);
    }
    if (m_Dc && m_Hwnd) {
        ReleaseDC(m_Hwnd, m_Dc);
    }
}

void WGLGraphicsContext::Init() {
    m_Dc = GetDC(m_Hwnd);
    if (!m_Dc) {
        std::cerr << "Engine Fatal: GetDC failed for WGL Context\n";
        std::exit(1);
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(m_Dc, &pfd);
    if (format == 0 || !SetPixelFormat(m_Dc, format, &pfd)) {
        std::cerr << "Engine Fatal: Failed to set pixel format for WGL Context\n";
        std::exit(1);
    }

    m_Context = wglCreateContext(m_Dc);
    if (!m_Context) {
        std::cerr << "Engine Fatal: Failed to create WGL Context\n";
        std::exit(1);
    }

    if (!wglMakeCurrent(m_Dc, m_Context)) {
        std::cerr << "Engine Fatal: wglMakeCurrent failed\n";
        std::exit(1);
    }

    RECT client{};
    GetClientRect(m_Hwnd, &client);
    m_Width = client.right - client.left;
    m_Height = client.bottom - client.top;

    // Screen-Space Projection (legacy DrawDebugQuad path, same as GLX)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, m_Width, m_Height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    std::cout << "OpenGL Context Initialized Successfully (WGL)\n";
}

void WGLGraphicsContext::SwapBuffers() {
    // Qualified: the unqualified name would resolve to this member.
    ::SwapBuffers(m_Dc);
}

void WGLGraphicsContext::SetClearColor(float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
}

void WGLGraphicsContext::DrawDebugQuad(const Transform2D& transform, const Vector2& size, const Color& color) {
    Vector2 halfSize = size * 0.5f;

    Vector2 corners[4] = {
        {-halfSize.x, -halfSize.y},
        {halfSize.x, -halfSize.y},
        {halfSize.x, halfSize.y},
        {-halfSize.x, halfSize.y}
    };

    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    for (const Vector2& corner : corners) {
        Vector2 world = transform.position + corner.Rotated(transform.rotation);
        glVertex2f(world.x, world.y);
    }
    glEnd();
}

#endif // _WIN32