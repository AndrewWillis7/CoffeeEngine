#include "GLLoader.h"
#include <iostream>
#include <cstdint>

#if defined(__linux__)
    #include <GL/glx.h>
namespace {
void* GetPlatformProcAddress(const char* name) {
    return reinterpret_cast<void*>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
}
}
#elif defined (_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
namespace {
void* GetPlatformProcAddress(const char* name) {
    // wglGetProcAddress signals failure with 0, 1, 2, 3 or -1 depending on
    // the driver, and never resolves GL 1.1 functions (those live in
    // opengl32.dll itself) -- fall back to the DLL's exports in that case.
    PROC p = wglGetProcAddress(name);
    auto v = reinterpret_cast<std::intptr_t>(p);
    if (v == 0 || v == 1 || v == 2 || v == 3 || v == -1) {
        static HMODULE s_OpenGL32 = LoadLibraryA("opengl32.dll");
        p = s_OpenGL32 ? GetProcAddress(s_OpenGL32, name) : nullptr;
    }
    return reinterpret_cast<void*>(p);
}
}
#else
namespace {
void* GetPlatformProcAddress(const char*) {return nullptr;}
}
#endif

namespace GL {

    PFNGLCREATESHADERPROC CreateShader = nullptr;
    PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC CompileShader = nullptr;
    PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC DeleteShader = nullptr;

    PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
    PFNGLATTACHSHADERPROC AttachShader = nullptr;
    PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
    PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
    PFNGLUSEPROGRAMPROC UseProgram = nullptr;

    PFNGLGENBUFFERSPROC GenBuffers = nullptr;
    PFNGLBINDBUFFERPROC BindBuffer = nullptr;
    PFNGLBUFFERDATAPROC BufferData = nullptr;
    PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;

    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray = nullptr;

    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
    PFNGLGETATTRIBLOCATIONPROC GetAttribLocation = nullptr;
    PFNGLUNIFORM1FPROC Uniform1f = nullptr;
    PFNGLUNIFORM2FPROC Uniform2f = nullptr;
    PFNGLUNIFORM3FPROC Uniform3f = nullptr;
    PFNGLUNIFORM4FPROC Uniform4f = nullptr;
    PFNGLUNIFORM1IPROC Uniform1i = nullptr;

namespace {
template <typename FnPtr>
bool LoadOne(FnPtr& outFn, const char* name) {
    outFn = reinterpret_cast<FnPtr>(GetPlatformProcAddress(name));
    if (!outFn) {
        std::cerr << "Engine Warning: Failed to load GL function: " << name << "\n";
        return false;
    }
    return true;
}
}

bool Load() {
    bool ok = true;

    ok &= LoadOne(CreateShader, "glCreateShader");
    ok &= LoadOne(ShaderSource, "glShaderSource");
    ok &= LoadOne(CompileShader, "glCompileShader");
    ok &= LoadOne(GetShaderiv, "glGetShaderiv");
    ok &= LoadOne(GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= LoadOne(DeleteShader, "glDeleteShader");

    ok &= LoadOne(CreateProgram, "glCreateProgram");
    ok &= LoadOne(AttachShader, "glAttachShader");
    ok &= LoadOne(LinkProgram, "glLinkProgram");
    ok &= LoadOne(GetProgramiv, "glGetProgramiv");
    ok &= LoadOne(GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= LoadOne(DeleteProgram, "glDeleteProgram");
    ok &= LoadOne(UseProgram, "glUseProgram");

    ok &= LoadOne(GenBuffers, "glGenBuffers");
    ok &= LoadOne(BindBuffer, "glBindBuffer");
    ok &= LoadOne(BufferData, "glBufferData");
    ok &= LoadOne(DeleteBuffers, "glDeleteBuffers");

    ok &= LoadOne(VertexAttribPointer, "glVertexAttribPointer");
    ok &= LoadOne(EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= LoadOne(DisableVertexAttribArray, "glDisableVertexAttribArray");

    ok &= LoadOne(GetUniformLocation, "glGetUniformLocation");
    ok &= LoadOne(GetAttribLocation, "glGetAttribLocation");
    ok &= LoadOne(Uniform1f, "glUniform1f");
    ok &= LoadOne(Uniform2f, "glUniform2f");
    ok &= LoadOne(Uniform3f, "glUniform3f");
    ok &= LoadOne(Uniform4f, "glUniform4f");
    ok &= LoadOne(Uniform1i, "glUniform1i");

    return ok;
}

} // End of Namespace GL