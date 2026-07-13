// mw-texdump: lists texture packs in a chunked file and exports textures as
// PNG. M2 deliverable (CLAUDE.md §6).
//
// Usage: mw-texdump <file> [--out <dir>] [--decompress] [--list]
//   --list        only print the pack/texture table (default when no --out)
//   --out <dir>   decode DXT1/3/5 and RGBA32 textures and write PNGs
//   --decompress  decompress a JDLZ/RefPack outer stream first

#include "openmw05/core/Stream.h"
#include "openmw05/formats/TexturePack.h"
#include "openmw05/gfx/DxtDecode.h"
#include "openmw05/io/Compression.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace {

const char* formatName(omw05::formats::TextureFormat f) {
    using TF = omw05::formats::TextureFormat;
    switch (f) {
    case TF::P4: return "P4";
    case TF::P8: return "P8";
    case TF::Rgb16: return "RGB16";
    case TF::Rgba16_1555: return "RGBA1555";
    case TF::Rgb16_565: return "RGB565";
    case TF::Rgba16_3555: return "RGBA3555";
    case TF::Rgb24: return "RGB24";
    case TF::Rgba32: return "RGBA32";
    case TF::Dxt1: return "DXT1";
    case TF::Dxt3: return "DXT3";
    case TF::Dxt5: return "DXT5";
    case TF::L8: return "L8";
    default: return "?";
    }
}

bool writePng(const std::string& path, const std::vector<std::uint8_t>& rgba, int w, int h) {
    return stbi_write_png(path.c_str(), w, h, 4, rgba.data(), w * 4) != 0;
}

std::string sanitize(const std::string& name) {
    std::string out = name.empty() ? "unnamed" : name;
    for (char& c : out) {
        if (!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            c = '_';
        }
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    const char* path = nullptr;
    const char* outDir = nullptr;
    bool tryDecompress = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--out") && i + 1 < argc) {
            outDir = argv[++i];
        } else if (!std::strcmp(argv[i], "--decompress")) {
            tryDecompress = true;
        } else if (!std::strcmp(argv[i], "--list")) {
            outDir = nullptr;
        } else if (!path) {
            path = argv[i];
        }
    }
    if (!path) {
        std::fprintf(stderr, "usage: mw-texdump <file> [--out <dir>] [--decompress]\n");
        return 2;
    }

    auto file = omw05::core::readFile(path);
    if (!file) {
        std::fprintf(stderr, "error: %s\n", file.error().message.c_str());
        return 1;
    }
    std::vector<std::uint8_t> bytes = file.take();
    omw05::core::ByteSpan span(bytes.data(), bytes.size());
    std::vector<std::uint8_t> decompressed;
    if (tryDecompress) {
        auto d = omw05::io::decompressAuto(span);
        if (d.ok()) {
            decompressed = d.take();
            span = omw05::core::ByteSpan(decompressed.data(), decompressed.size());
        }
    }

    auto packsResult = omw05::formats::parseTexturePacks(span);
    if (!packsResult) {
        std::fprintf(stderr, "error: %s\n", packsResult.error().message.c_str());
        return 1;
    }
    const auto& packs = packsResult.value();
    std::printf("%zu texture pack(s)\n", packs.size());

    int exported = 0;
    int skipped = 0;
    for (const auto& pack : packs) {
        std::printf("pack '%s' (file '%s', key 0x%08X): %zu textures\n", pack.name.c_str(),
                    pack.filename.c_str(), pack.key, pack.textures.size());
        for (const auto& tex : pack.textures) {
            std::printf("  %-24s 0x%08X %4ux%-4u %-8s mips=%u size=%u\n", tex.name.c_str(),
                        tex.nameHash, tex.width, tex.height, formatName(tex.textureFormat()),
                        tex.mipmaps, tex.dataSize);
            if (!outDir) {
                continue;
            }
            using TF = omw05::formats::TextureFormat;
            std::vector<std::uint8_t> rgba;
            const TF f = tex.textureFormat();
            if (f == TF::Dxt1) {
                rgba = omw05::gfx::decodeDxt(tex.data, tex.width, tex.height, 1);
            } else if (f == TF::Dxt3) {
                rgba = omw05::gfx::decodeDxt(tex.data, tex.width, tex.height, 3);
            } else if (f == TF::Dxt5) {
                rgba = omw05::gfx::decodeDxt(tex.data, tex.width, tex.height, 5);
            } else if (f == TF::Rgba32 &&
                       tex.data.size() >= static_cast<std::size_t>(tex.width) * tex.height * 4) {
                rgba.assign(tex.data.begin(),
                            tex.data.begin() + static_cast<std::size_t>(tex.width) * tex.height * 4);
                for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {  // BGRA -> RGBA
                    std::swap(rgba[i], rgba[i + 2]);
                }
            } else {
                ++skipped;
                continue;
            }
            if (rgba.empty()) {
                ++skipped;
                continue;
            }
            const std::string out =
                std::string(outDir) + "/" + sanitize(pack.name) + "_" + sanitize(tex.name) + ".png";
            if (writePng(out, rgba, tex.width, tex.height)) {
                ++exported;
            } else {
                std::fprintf(stderr, "failed to write %s\n", out.c_str());
            }
        }
    }
    if (outDir) {
        std::printf("exported %d PNG(s), skipped %d unsupported\n", exported, skipped);
    }
    return 0;
}
