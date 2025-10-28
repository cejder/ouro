#include "render.hpp"
#include "log.hpp"

#include <external/glfw/include/GLFW/glfw3.h>
#include <external/glad.h>

// Bindless texture extension function pointers (GL_ARB_bindless_texture)
// These allow unlimited textures without binding to texture units
using PFNGLGETTEXTUREHANDLEARBPROC             = GLuint64 (*)(GLuint texture);
using PFNGLMAKETEXTUREHANDLERESIDENTARBPROC    = void (*)(GLuint64 handle);
using PFNGLMAKETEXTUREHANDLENONRESIDENTARBPROC = void (*)(GLuint64 handle);

PFNGLGETTEXTUREHANDLEARBPROC             static glGetTextureHandleARB             = nullptr;
PFNGLMAKETEXTUREHANDLERESIDENTARBPROC    static glMakeTextureHandleResidentARB    = nullptr;
PFNGLMAKETEXTUREHANDLENONRESIDENTARBPROC static glMakeTextureHandleNonResidentARB = nullptr;

// Load GL_ARB_bindless_texture extension functions from driver
// Returns true if extension is supported, false otherwise
BOOL render_load_bindless_texture_extension() {
    // Query function pointers from OpenGL driver
    glGetTextureHandleARB             = (PFNGLGETTEXTUREHANDLEARBPROC)glfwGetProcAddress("glGetTextureHandleARB");
    glMakeTextureHandleResidentARB    = (PFNGLMAKETEXTUREHANDLERESIDENTARBPROC)glfwGetProcAddress("glMakeTextureHandleResidentARB");
    glMakeTextureHandleNonResidentARB = (PFNGLMAKETEXTUREHANDLENONRESIDENTARBPROC)glfwGetProcAddress("glMakeTextureHandleNonResidentARB");

    // Verify all required functions are available
    if (!glGetTextureHandleARB || !glMakeTextureHandleResidentARB || !glMakeTextureHandleNonResidentARB) {
        lle("GL_ARB_bindless_texture extension not available! 3D Particle system requires this extension.");
        return false;
    }

    return true;
}

// Wrapper functions to access bindless texture extension
U64 render_get_texture_handle(U32 texture_id) {
    return glGetTextureHandleARB(texture_id);
}

void render_make_texture_resident(U64 handle) {
    glMakeTextureHandleResidentARB(handle);
}

void render_make_texture_nonresident(U64 handle) {
    glMakeTextureHandleNonResidentARB(handle);
}
