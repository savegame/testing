#pragma once
// SDL2 window + OpenGL ES 3.0 context (CLAUDE.md §2: SDL2, ES 3.0 profile,
// GLES3 API surface only — loaded via glad generated for gles2/3.0).
//
// Ownership: GlContext owns the SDL_Window and SDL_GLContext and destroys
// them (and calls SDL_Quit) on destruction. Move-only.

#include "openmw05/core/Result.h"

struct SDL_Window;

namespace omw05 {
namespace gfx {

class GlContext {
public:
    // Initializes SDL video, creates a resizable window and an ES 3.0 context
    // (SDL_GL_CONTEXT_PROFILE_ES, major 3, minor 0), makes it current and
    // loads GLES entry points through glad.
    static core::Result<GlContext> create(const char* title, int width, int height);

    GlContext(GlContext&& other) noexcept;
    GlContext& operator=(GlContext&& other) noexcept;
    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;
    ~GlContext();

    SDL_Window* window() const { return window_; }

    // Physical drawable size in pixels (may differ from window size on HiDPI).
    void drawableSize(int* width, int* height) const;

    void swap() const;
    void setVsync(bool enabled) const;

private:
    GlContext(SDL_Window* window, void* glContext)
        : window_(window), glContext_(glContext) {}

    SDL_Window* window_ = nullptr;
    void* glContext_ = nullptr;  // SDL_GLContext
};

} // namespace gfx
} // namespace omw05
