#pragma once
// CPU decoding of TexturePack entries to RGBA8. Header-only, GL-free —
// shared by tools (mw-texdump, mw-cardump) and the viewer upload path.
//
// Format knowledge: TextureCompressionType values from Nikki; DXT via
// gfx/DxtDecode.h; palettized/16-bit layouts are the standard PC formats
// (palette entries stored BGRA8, pixels little-endian).

#include "openmw05/formats/TexturePack.h"
#include "openmw05/gfx/DxtDecode.h"

#include <cstdint>
#include <vector>

namespace omw05 {
namespace formats {

inline const char* textureFormatName(TextureFormat f) {
    switch (f) {
    case TextureFormat::P4: return "P4";
    case TextureFormat::P8: return "P8";
    case TextureFormat::Rgb16: return "RGB16";
    case TextureFormat::Rgba16_1555: return "RGBA1555";
    case TextureFormat::Rgb16_565: return "RGB565";
    case TextureFormat::Rgba16_3555: return "RGBA3555";
    case TextureFormat::Rgb24: return "RGB24";
    case TextureFormat::Rgba32: return "RGBA32";
    case TextureFormat::Dxt: return "DXT";
    case TextureFormat::Dxt1: return "DXT1";
    case TextureFormat::Dxt3: return "DXT3";
    case TextureFormat::Dxt5: return "DXT5";
    case TextureFormat::DxtN: return "DXTN";
    case TextureFormat::L8: return "L8";
    default: return "?";
    }
}

// Decodes the top mip level to RGBA8 (w*h*4). Empty result = unsupported
// format or malformed data.
inline std::vector<std::uint8_t> decodeTextureRgba(const TextureEntry& tex) {
    const std::size_t pixels = static_cast<std::size_t>(tex.width) * tex.height;
    std::vector<std::uint8_t> out;
    if (pixels == 0) {
        return out;
    }

    auto bgraToRgba = [](std::uint32_t bgra, std::uint8_t* dst) {
        dst[0] = static_cast<std::uint8_t>(bgra >> 16);
        dst[1] = static_cast<std::uint8_t>(bgra >> 8);
        dst[2] = static_cast<std::uint8_t>(bgra);
        dst[3] = static_cast<std::uint8_t>(bgra >> 24);
    };

    switch (tex.textureFormat()) {
    case TextureFormat::Dxt1:
        return gfx::decodeDxt(tex.data, tex.width, tex.height, 1);
    case TextureFormat::Dxt3:
        return gfx::decodeDxt(tex.data, tex.width, tex.height, 3);
    case TextureFormat::Dxt5:
        return gfx::decodeDxt(tex.data, tex.width, tex.height, 5);

    case TextureFormat::Rgba32:
        if (tex.data.size() >= pixels * 4) {
            out.resize(pixels * 4);
            for (std::size_t i = 0; i < pixels; ++i) {
                bgraToRgba(core::readLe32(tex.data.data() + i * 4), &out[i * 4]);
            }
        }
        return out;

    case TextureFormat::P8:
        if (tex.data.size() >= pixels && tex.palette.size() >= 4) {
            out.resize(pixels * 4);
            const std::size_t palEntries = tex.palette.size() / 4;
            for (std::size_t i = 0; i < pixels; ++i) {
                const std::size_t idx = tex.data[i] < palEntries ? tex.data[i] : 0;
                bgraToRgba(core::readLe32(tex.palette.data() + idx * 4), &out[i * 4]);
            }
        }
        return out;

    case TextureFormat::P4:
        if (tex.data.size() >= (pixels + 1) / 2 && tex.palette.size() >= 4) {
            out.resize(pixels * 4);
            const std::size_t palEntries = tex.palette.size() / 4;
            for (std::size_t i = 0; i < pixels; ++i) {
                const std::uint8_t byte = tex.data[i / 2];
                std::size_t idx = (i & 1) ? (byte >> 4) : (byte & 0x0F);
                if (idx >= palEntries) {
                    idx = 0;
                }
                bgraToRgba(core::readLe32(tex.palette.data() + idx * 4), &out[i * 4]);
            }
        }
        return out;

    case TextureFormat::Rgb16_565:
        if (tex.data.size() >= pixels * 2) {
            out.resize(pixels * 4);
            for (std::size_t i = 0; i < pixels; ++i) {
                const std::uint16_t v = core::readLe16(tex.data.data() + i * 2);
                const std::uint8_t r5 = (v >> 11) & 0x1F;
                const std::uint8_t g6 = (v >> 5) & 0x3F;
                const std::uint8_t b5 = v & 0x1F;
                out[i * 4 + 0] = static_cast<std::uint8_t>((r5 << 3) | (r5 >> 2));
                out[i * 4 + 1] = static_cast<std::uint8_t>((g6 << 2) | (g6 >> 4));
                out[i * 4 + 2] = static_cast<std::uint8_t>((b5 << 3) | (b5 >> 2));
                out[i * 4 + 3] = 255;
            }
        }
        return out;

    case TextureFormat::Rgba16_1555:
        if (tex.data.size() >= pixels * 2) {
            out.resize(pixels * 4);
            for (std::size_t i = 0; i < pixels; ++i) {
                const std::uint16_t v = core::readLe16(tex.data.data() + i * 2);
                const std::uint8_t r5 = (v >> 10) & 0x1F;
                const std::uint8_t g5 = (v >> 5) & 0x1F;
                const std::uint8_t b5 = v & 0x1F;
                out[i * 4 + 0] = static_cast<std::uint8_t>((r5 << 3) | (r5 >> 2));
                out[i * 4 + 1] = static_cast<std::uint8_t>((g5 << 3) | (g5 >> 2));
                out[i * 4 + 2] = static_cast<std::uint8_t>((b5 << 3) | (b5 >> 2));
                out[i * 4 + 3] = (v & 0x8000) ? 255 : 0;
            }
        }
        return out;

    case TextureFormat::Rgb24:
        if (tex.data.size() >= pixels * 3) {
            out.resize(pixels * 4);
            for (std::size_t i = 0; i < pixels; ++i) {  // stored BGR
                out[i * 4 + 0] = tex.data[i * 3 + 2];
                out[i * 4 + 1] = tex.data[i * 3 + 1];
                out[i * 4 + 2] = tex.data[i * 3 + 0];
                out[i * 4 + 3] = 255;
            }
        }
        return out;

    case TextureFormat::L8:
        if (tex.data.size() >= pixels) {
            out.resize(pixels * 4);
            for (std::size_t i = 0; i < pixels; ++i) {
                out[i * 4 + 0] = out[i * 4 + 1] = out[i * 4 + 2] = tex.data[i];
                out[i * 4 + 3] = 255;
            }
        }
        return out;

    default:
        return out;  // unsupported (DXTN normal maps, 3555, ...)
    }
}

} // namespace formats
} // namespace omw05
