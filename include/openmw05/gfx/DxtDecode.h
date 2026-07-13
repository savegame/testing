#pragma once
// CPU DXT1/3/5 (BC1/2/3) -> RGBA8 decoder (CLAUDE.md §5: GLES3 devices
// often lack GL_EXT_texture_compression_s3tc, so a CPU fallback is
// mandatory). Header-only and GL-free on purpose: tools (mw-texdump) and
// tests use it headless; gfx uploads its output.
//
// Algorithm from the public S3TC/BC1-3 specification (Khronos Data Format /
// EXT_texture_compression_s3tc extension text).

#include "openmw05/core/Span17.h"

#include <cstdint>
#include <vector>

namespace omw05 {
namespace gfx {

namespace detail {

inline void decodeColorBlockDxt(const std::uint8_t* block, std::uint8_t out[16][4],
                                bool dxt1Alpha) {
    const std::uint16_t c0 = static_cast<std::uint16_t>(block[0] | (block[1] << 8));
    const std::uint16_t c1 = static_cast<std::uint16_t>(block[2] | (block[3] << 8));
    std::uint8_t colors[4][4];
    auto expand565 = [](std::uint16_t c, std::uint8_t* rgb) {
        const std::uint8_t r5 = (c >> 11) & 0x1F;
        const std::uint8_t g6 = (c >> 5) & 0x3F;
        const std::uint8_t b5 = c & 0x1F;
        rgb[0] = static_cast<std::uint8_t>((r5 << 3) | (r5 >> 2));
        rgb[1] = static_cast<std::uint8_t>((g6 << 2) | (g6 >> 4));
        rgb[2] = static_cast<std::uint8_t>((b5 << 3) | (b5 >> 2));
    };
    expand565(c0, colors[0]);
    expand565(c1, colors[1]);
    colors[0][3] = colors[1][3] = 255;
    if (c0 > c1 || !dxt1Alpha) {  // 4-color mode (always for DXT3/5 blocks)
        for (int i = 0; i < 3; ++i) {
            colors[2][i] = static_cast<std::uint8_t>((2 * colors[0][i] + colors[1][i]) / 3);
            colors[3][i] = static_cast<std::uint8_t>((colors[0][i] + 2 * colors[1][i]) / 3);
        }
        colors[2][3] = colors[3][3] = 255;
    } else {  // 3-color + transparent mode (DXT1 only)
        for (int i = 0; i < 3; ++i) {
            colors[2][i] = static_cast<std::uint8_t>((colors[0][i] + colors[1][i]) / 2);
            colors[3][i] = 0;
        }
        colors[2][3] = 255;
        colors[3][3] = 0;
    }
    const std::uint32_t bits = static_cast<std::uint32_t>(block[4]) |
                               (static_cast<std::uint32_t>(block[5]) << 8) |
                               (static_cast<std::uint32_t>(block[6]) << 16) |
                               (static_cast<std::uint32_t>(block[7]) << 24);
    for (int i = 0; i < 16; ++i) {
        const std::uint32_t idx = (bits >> (2 * i)) & 3;
        out[i][0] = colors[idx][0];
        out[i][1] = colors[idx][1];
        out[i][2] = colors[idx][2];
        out[i][3] = colors[idx][3];
    }
}

inline void decodeAlphaBlockDxt5(const std::uint8_t* block, std::uint8_t alpha[16]) {
    const std::uint8_t a0 = block[0];
    const std::uint8_t a1 = block[1];
    std::uint8_t table[8];
    table[0] = a0;
    table[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i < 7; ++i) {
            table[1 + i] = static_cast<std::uint8_t>(((7 - i) * a0 + i * a1) / 7);
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            table[1 + i] = static_cast<std::uint8_t>(((5 - i) * a0 + i * a1) / 5);
        }
        table[6] = 0;
        table[7] = 255;
    }
    std::uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) {
        bits |= static_cast<std::uint64_t>(block[2 + i]) << (8 * i);
    }
    for (int i = 0; i < 16; ++i) {
        alpha[i] = table[(bits >> (3 * i)) & 7];
    }
}

} // namespace detail

// Decodes a full DXT1/3/5 mip level of `width`x`height` pixels into RGBA8
// (row-major, 4 bytes/px). Returns empty vector when `data` is too small.
// `format`: 1 = DXT1, 3 = DXT3, 5 = DXT5.
inline std::vector<std::uint8_t> decodeDxt(core::ByteSpan data, int width, int height,
                                           int format) {
    if (width <= 0 || height <= 0 || (format != 1 && format != 3 && format != 5)) {
        return {};
    }
    const int bw = (width + 3) / 4;
    const int bh = (height + 3) / 4;
    const std::size_t blockSize = (format == 1) ? 8 : 16;
    if (data.size() < static_cast<std::size_t>(bw) * bh * blockSize) {
        return {};
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(width) * height * 4);

    const std::uint8_t* src = data.data();
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            std::uint8_t texels[16][4];
            std::uint8_t alpha[16];
            if (format == 1) {
                detail::decodeColorBlockDxt(src, texels, true);
            } else if (format == 3) {
                for (int i = 0; i < 16; ++i) {  // 4-bit explicit alpha
                    const std::uint8_t byte = src[i / 2];
                    const std::uint8_t nib = (i & 1) ? (byte >> 4) : (byte & 0x0F);
                    alpha[i] = static_cast<std::uint8_t>(nib * 17);
                }
                detail::decodeColorBlockDxt(src + 8, texels, false);
                for (int i = 0; i < 16; ++i) {
                    texels[i][3] = alpha[i];
                }
            } else {
                detail::decodeAlphaBlockDxt5(src, alpha);
                detail::decodeColorBlockDxt(src + 8, texels, false);
                for (int i = 0; i < 16; ++i) {
                    texels[i][3] = alpha[i];
                }
            }
            src += blockSize;

            for (int py = 0; py < 4; ++py) {
                const int y = by * 4 + py;
                if (y >= height) {
                    break;
                }
                for (int px = 0; px < 4; ++px) {
                    const int x = bx * 4 + px;
                    if (x >= width) {
                        continue;
                    }
                    std::uint8_t* dst = &out[(static_cast<std::size_t>(y) * width + x) * 4];
                    dst[0] = texels[py * 4 + px][0];
                    dst[1] = texels[py * 4 + px][1];
                    dst[2] = texels[py * 4 + px][2];
                    dst[3] = texels[py * 4 + px][3];
                }
            }
        }
    }
    return out;
}

} // namespace gfx
} // namespace omw05
