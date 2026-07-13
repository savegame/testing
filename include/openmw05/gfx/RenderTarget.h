#pragma once
// Offscreen FBO wrapper (CLAUDE.md §5a). Nothing except the Compositor's
// final pass ever draws to the default framebuffer; all rendering goes
// through RenderTargets:
//   - sceneRT: 3D world at window size * render-scale
//   - uiRT:    2D/UI at native logical resolution, cleared to transparent
//
// RAII over the FBO, its RGBA8 (or sRGB8_ALPHA8) color texture and optional
// DEPTH24_STENCIL8 renderbuffer. Requires a current GL context. Move-only.

#include "openmw05/core/Result.h"

#include <cstdint>

namespace omw05 {
namespace gfx {

class RenderTarget {
public:
    struct Desc {
        int width = 0;
        int height = 0;
        bool srgb = false;          // GL_SRGB8_ALPHA8 instead of GL_RGBA8
        bool depthStencil = false;  // attach a DEPTH24_STENCIL8 renderbuffer
    };

    // Creates FBO + attachments and verifies GLES3 framebuffer completeness.
    static core::Result<RenderTarget> create(const Desc& desc);

    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    ~RenderTarget();

    // Reallocates attachments if the size actually changed. No-op otherwise.
    core::Result<void> resize(int width, int height);

    // Binds the FBO and sets the viewport to the full target.
    void bind() const;
    // Binds the default framebuffer again (viewport left to the caller).
    static void bindDefault();

    int width() const { return desc_.width; }
    int height() const { return desc_.height; }
    std::uint32_t colorTexture() const { return colorTex_; }

private:
    RenderTarget() = default;
    void destroy();
    core::Result<void> allocate();

    Desc desc_{};
    std::uint32_t fbo_ = 0;
    std::uint32_t colorTex_ = 0;
    std::uint32_t depthRbo_ = 0;
};

} // namespace gfx
} // namespace omw05
