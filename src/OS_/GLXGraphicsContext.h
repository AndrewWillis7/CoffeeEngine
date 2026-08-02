#pragma once
#include "IGraphicsContext.h"

// Forward Declaration of X11/GLX
struct _XDisplay;
struct __GLXcontextRec;

class GLXGraphicsContext : public IGraphicsContext {
public:
    GLXGraphicsContext(void* display, void* window);
    ~GLXGraphicsContext() override;

    void Init() override;
    void SwapBuffers() override;
    void SetClearColor(float r, float g, float b) override;
    void DrawDebugQuad(const Transform2D& transform, const Vector2& size, const Color& color) override;

private:
    _XDisplay* m_Display;
    unsigned long m_Window;
    __GLXcontextRec* m_Context;
    int m_Width = 0;
    int m_Height = 0;
};