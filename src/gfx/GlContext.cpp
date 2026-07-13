#include "openmw05/gfx/GlContext.h"

#include "openmw05/core/Log.h"

#include <SDL.h>
#include <glad/gles2.h>

namespace omw05 {
namespace gfx {

core::Result<GlContext> GlContext::create(const char* title, int width, int height) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        return core::Error{core::ErrorCode::GraphicsError,
                           std::string("SDL_Init(VIDEO) failed: ") + SDL_GetError()};
    }

    // Hard constraint: OpenGL ES 3.0 profile (CLAUDE.md §2).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);  // depth lives on the FBOs, not the default FB

    SDL_Window* window =
        SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        return core::Error{core::ErrorCode::GraphicsError,
                           std::string("SDL_CreateWindow failed: ") + SDL_GetError()};
    }

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        SDL_DestroyWindow(window);
        return core::Error{core::ErrorCode::GraphicsError,
                           std::string("SDL_GL_CreateContext (ES 3.0) failed: ") + SDL_GetError()};
    }
    SDL_GL_MakeCurrent(window, gl);

    if (gladLoadGLES2(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) == 0) {
        SDL_GL_DeleteContext(gl);
        SDL_DestroyWindow(window);
        return core::Error{core::ErrorCode::GraphicsError, "gladLoadGLES2 failed"};
    }

    OMW05_LOG_INFO("gfx", "GL_VENDOR:   %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    OMW05_LOG_INFO("gfx", "GL_RENDERER: %s",
                   reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    OMW05_LOG_INFO("gfx", "GL_VERSION:  %s",
                   reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    return GlContext(window, gl);
}

GlContext::GlContext(GlContext&& other) noexcept
    : window_(other.window_), glContext_(other.glContext_) {
    other.window_ = nullptr;
    other.glContext_ = nullptr;
}

GlContext& GlContext::operator=(GlContext&& other) noexcept {
    if (this != &other) {
        this->~GlContext();
        window_ = other.window_;
        glContext_ = other.glContext_;
        other.window_ = nullptr;
        other.glContext_ = nullptr;
    }
    return *this;
}

GlContext::~GlContext() {
    if (glContext_) {
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(glContext_));
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

void GlContext::drawableSize(int* width, int* height) const {
    SDL_GL_GetDrawableSize(window_, width, height);
}

void GlContext::swap() const { SDL_GL_SwapWindow(window_); }

void GlContext::setVsync(bool enabled) const { SDL_GL_SetSwapInterval(enabled ? 1 : 0); }

} // namespace gfx
} // namespace omw05
