#include "openmw05/gfx/Texture.h"

#include <glad/gles2.h>

namespace omw05 {
namespace gfx {

core::Result<Texture> Texture::createRgba8(int width, int height, const std::uint8_t* pixels) {
    if (width <= 0 || height <= 0 || !pixels) {
        return core::Error{core::ErrorCode::InvalidArgument, "bad texture upload"};
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return Texture(tex, width, height);
}

Texture::Texture(Texture&& other) noexcept
    : tex_(other.tex_), width_(other.width_), height_(other.height_) {
    other.tex_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (tex_) {
            glDeleteTextures(1, &tex_);
        }
        tex_ = other.tex_;
        width_ = other.width_;
        height_ = other.height_;
        other.tex_ = 0;
    }
    return *this;
}

Texture::~Texture() {
    if (tex_) {
        glDeleteTextures(1, &tex_);
    }
}

} // namespace gfx
} // namespace omw05
