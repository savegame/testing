#include "openmw05/gfx/RenderTarget.h"

#include <glad/gles2.h>

#include <string>

namespace omw05 {
namespace gfx {

core::Result<RenderTarget> RenderTarget::create(const Desc& desc) {
    if (desc.width <= 0 || desc.height <= 0) {
        return core::Error{core::ErrorCode::InvalidArgument, "RenderTarget size must be positive"};
    }
    RenderTarget rt;
    rt.desc_ = desc;
    core::Result<void> alloc = rt.allocate();
    if (!alloc) {
        return alloc.error();
    }
    return core::Result<RenderTarget>(std::move(rt));
}

core::Result<void> RenderTarget::allocate() {
    destroy();

    glGenTextures(1, &colorTex_);
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    const GLenum internalFormat = desc_.srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, desc_.width, desc_.height);
    // Linear so the compositor's render-scale upscale looks acceptable (§5a).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (desc_.depthStencil) {
        glGenRenderbuffers(1, &depthRbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, desc_.width, desc_.height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);
    if (depthRbo_) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  depthRbo_);
    }

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        destroy();
        return core::Error{core::ErrorCode::GraphicsError,
                           "framebuffer incomplete, status 0x" + std::to_string(status)};
    }
    return core::Result<void>();
}

core::Result<void> RenderTarget::resize(int width, int height) {
    if (width == desc_.width && height == desc_.height) {
        return core::Result<void>();
    }
    if (width <= 0 || height <= 0) {
        return core::Error{core::ErrorCode::InvalidArgument, "RenderTarget size must be positive"};
    }
    desc_.width = width;
    desc_.height = height;
    return allocate();
}

void RenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, desc_.width, desc_.height);
}

void RenderTarget::bindDefault() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void RenderTarget::destroy() {
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (colorTex_) {
        glDeleteTextures(1, &colorTex_);
        colorTex_ = 0;
    }
    if (depthRbo_) {
        glDeleteRenderbuffers(1, &depthRbo_);
        depthRbo_ = 0;
    }
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : desc_(other.desc_), fbo_(other.fbo_), colorTex_(other.colorTex_),
      depthRbo_(other.depthRbo_) {
    other.fbo_ = 0;
    other.colorTex_ = 0;
    other.depthRbo_ = 0;
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
    if (this != &other) {
        destroy();
        desc_ = other.desc_;
        fbo_ = other.fbo_;
        colorTex_ = other.colorTex_;
        depthRbo_ = other.depthRbo_;
        other.fbo_ = 0;
        other.colorTex_ = 0;
        other.depthRbo_ = 0;
    }
    return *this;
}

RenderTarget::~RenderTarget() { destroy(); }

} // namespace gfx
} // namespace omw05
