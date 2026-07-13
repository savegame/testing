#pragma once
// GL texture wrapper (GLES3). Owns the GL object; requires a current
// context. formats/ never touches this — decoded CPU pixels come in,
// a GL texture comes out. Move-only.

#include "openmw05/core/Result.h"

#include <cstdint>

namespace omw05 {
namespace gfx {

class Texture {
public:
    // Uploads RGBA8 pixels (w*h*4 bytes, row-major, no padding).
    static core::Result<Texture> createRgba8(int width, int height, const std::uint8_t* pixels);

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    ~Texture();

    std::uint32_t id() const { return tex_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    Texture(std::uint32_t tex, int w, int h) : tex_(tex), width_(w), height_(h) {}
    std::uint32_t tex_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace gfx
} // namespace omw05
