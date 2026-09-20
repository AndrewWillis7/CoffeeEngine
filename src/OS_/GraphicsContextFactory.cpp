#include "IGraphicsContext.h"

#ifdef _WIN32
    #include "WGLGraphicsContext.h"
#elif defined(__linux__)
    #include "GLXGraphicsContext.h"
#else  
    #error "UNKNOWN PLATFORM!"
#endif

std::unique_ptr<IGraphicsContext> IGraphicsContext::Create(void* nativeDisplay, void* nativeWindow) {
#ifdef _WIN32
    return std::make_unique<WGLGraphicsContext>(nativeDisplay, nativeWindow);
#elif defined(__linux__)
    return std::make_unique<GLXGraphicsContext>(nativeDisplay, nativeWindow);
#endif
}