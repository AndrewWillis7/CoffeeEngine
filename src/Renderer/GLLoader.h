#pragma once
#include <GL/gl.h>
// Enum values past GL 1.1 (GL_ARRAY_BUFFER, GL_VERTEX_SHADER,
// GL_CLAMP_TO_EDGE, ...). Mesa's gl.h already pulls this in; Windows'
// gl.h stops at 1.1, so include it explicitly. Only enums/typedefs --
// GL_GLEXT_PROTOTYPES is never defined, so no global prototypes leak in.
#include <GL/glext.h>
#include <cstddef>

// Entry points are __stdcall on 32-bit Windows; empty everywhere else.
#ifndef APIENTRY
    #define APIENTRY
#endif

// On Linux the ABI only guarentees libGL exports up through OpenGL 1.2
// Everything Shader related (GL 2.0+) has to be resolved at runtime
// glXGetProcAddressARB can do this

// On Windows opengl32.dll only exports GL 1.1, so the same applies there
// via wglGetProcAddress.

// Deliberately not GLAD/GLEW, just entrypoints

namespace GL {

using PFNGLCREATESHADERPROC = GLuint (APIENTRY *)(GLenum type);
using PFNGLSHADERSOURCEPROC = void (APIENTRY *)(GLuint shader, GLsizei count, const char* const* string, const GLint* length);
using PFNGLCOMPILESHADERPROC = void (APIENTRY *)(GLuint shader);
using PFNGLGETSHADERIVPROC = void (APIENTRY *)(GLuint shader, GLenum pname, GLint* params);
using PFNGLGETSHADERINFOLOGPROC = void (APIENTRY *)(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog);
using PFNGLDELETESHADERPROC = void (APIENTRY *)(GLuint shader);

using PFNGLCREATEPROGRAMPROC = GLuint (APIENTRY *)();
using PFNGLATTACHSHADERPROC = void (APIENTRY *)(GLuint program, GLuint shader);
using PFNGLLINKPROGRAMPROC = void (APIENTRY *)(GLuint program);
using PFNGLGETPROGRAMIVPROC = void (APIENTRY *)(GLuint program, GLenum pname, GLint* params);
using PFNGLGETPROGRAMINFOLOGPROC = void (APIENTRY *)(GLuint program, GLsizei bufSize, GLsizei* length, char* infoLog);
using PFNGLDELETEPROGRAMPROC = void (APIENTRY *)(GLuint program);
using PFNGLUSEPROGRAMPROC = void (APIENTRY *)(GLuint program);

using PFNGLGENBUFFERSPROC = void (APIENTRY *)(GLsizei n, GLuint* buffers);
using PFNGLBINDBUFFERPROC = void (APIENTRY *)(GLenum target, GLuint buffer);
using PFNGLBUFFERDATAPROC = void (APIENTRY *)(GLenum target, ptrdiff_t size, const void* data, GLenum usage);
using PFNGLDELETEBUFFERSPROC = void (APIENTRY *)(GLsizei n, const GLuint* buffers);

using PFNGLVERTEXATTRIBPOINTERPROC = void (APIENTRY *)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (APIENTRY *)(GLuint index);
using PFNGLDISABLEVERTEXATTRIBARRAYPROC = void (APIENTRY *)(GLuint index);

using PFNGLGETUNIFORMLOCATIONPROC = GLint (APIENTRY *)(GLuint program, const char* name);
using PFNGLGETATTRIBLOCATIONPROC = GLint (APIENTRY *)(GLuint program, const char* name);
using PFNGLUNIFORM1FPROC = void (APIENTRY *)(GLint location, GLfloat v0);
using PFNGLUNIFORM2FPROC = void (APIENTRY *)(GLint location, GLfloat v0, GLfloat v1);
using PFNGLUNIFORM3FPROC = void (APIENTRY *)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
using PFNGLUNIFORM4FPROC = void (APIENTRY *)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

extern PFNGLCREATESHADERPROC CreateShader;
extern PFNGLSHADERSOURCEPROC ShaderSource;
extern PFNGLCOMPILESHADERPROC CompileShader;
extern PFNGLGETSHADERIVPROC GetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
extern PFNGLDELETESHADERPROC DeleteShader;

extern PFNGLCREATEPROGRAMPROC CreateProgram;
extern PFNGLATTACHSHADERPROC AttachShader;
extern PFNGLLINKPROGRAMPROC LinkProgram;
extern PFNGLGETPROGRAMIVPROC GetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
extern PFNGLDELETEPROGRAMPROC DeleteProgram;
extern PFNGLUSEPROGRAMPROC UseProgram;

extern PFNGLGENBUFFERSPROC GenBuffers;
extern PFNGLBINDBUFFERPROC BindBuffer;
extern PFNGLBUFFERDATAPROC BufferData;
extern PFNGLDELETEBUFFERSPROC DeleteBuffers;

extern PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;

extern PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
extern PFNGLGETATTRIBLOCATIONPROC GetAttribLocation;
extern PFNGLUNIFORM1FPROC Uniform1f;
extern PFNGLUNIFORM2FPROC Uniform2f;
extern PFNGLUNIFORM3FPROC Uniform3f;
extern PFNGLUNIFORM4FPROC Uniform4f;

using PFNGLUNIFORM1IPROC = void (APIENTRY *)(GLint location, GLint v0);
extern PFNGLUNIFORM1IPROC Uniform1i;

// Resolves every entrypoint above against the GL Context
// Must be called after IGraphicsContext::Init()
bool Load();

}