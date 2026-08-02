#include "GLXGraphicsContext.h"
#include <iostream>

// Safe includes of OS/API specific headers
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/gl.h>

GLXGraphicsContext::GLXGraphicsContext(void* display, void* window)
    : m_Display(static_cast<_XDisplay*>(display)),
    // Cast the window back from void* safely
    m_Window(static_cast<unsigned long>(reinterpret_cast<uintptr_t>(window))),
    m_Context(nullptr) {}

GLXGraphicsContext::~GLXGraphicsContext() {
    if (m_Display && m_Context) {
        glXMakeCurrent(m_Display, None, nullptr);
        glXDestroyContext(m_Display, m_Context);
    }
}

void GLXGraphicsContext::Init() {
    // We need to fetch the visual information from the existing X11
    XWindowAttributes wa;
    XGetWindowAttributes(m_Display, m_Window, &wa);

    XVisualInfo vinfo_template;
    vinfo_template.visualid = XVisualIDFromVisual(wa.visual);

    int num_vinfo;
    XVisualInfo* vi = XGetVisualInfo(m_Display, VisualIDMask, &vinfo_template, &num_vinfo);

    if (!vi) {
        std::cerr << "Engine Fatal: Failed to get XVisualInfo for GLX Context\n";
        exit(1);
    }

    // Create the GLX Context
    m_Context = glXCreateContext(m_Display, vi, nullptr, GL_TRUE);
    if (!m_Context) {
        std::cerr << "Enginer Fatal: Failed to create GLX Context\n";
        exit(1);
    }

    // Bind the current context to windo so OpenGL knows where to draw
    glXMakeCurrent(m_Display, m_Window, m_Context);

    m_Width = wa.width;
    m_Height = wa.height;

    // Screen-Space Projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, m_Width, m_Height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    XFree(vi);
    std::cout << "OpenGL Context Initialized Successfully (GLX)\n";
}

void GLXGraphicsContext::SwapBuffers() {
    glXSwapBuffers(m_Display, m_Window);
}

void GLXGraphicsContext::SetClearColor(float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
}

void GLXGraphicsContext::DrawDebugQuad(const Transform2D& transform, const Vector2& size, const Color& color) {
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