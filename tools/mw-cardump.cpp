// mw-cardump: cross-references one car's GEOMETRY.BIN and TEXTURES.BIN —
// lists textures (format, decodability) and per-object materials with
// resolved texture names, flagging hashes that match no loaded texture.
// This is the tool for diagnosing untextured parts.
//
// Usage: mw-cardump --gamedir <dir> --car BMWM3GTRE46 [--markers]
//        mw-cardump <geometry.bin> <textures.bin> [--markers]

#include "openmw05/core/Stream.h"
#include "openmw05/formats/Solids.h"
#include "openmw05/formats/TextureDecode.h"
#include "openmw05/formats/TexturePack.h"
#include "openmw05/io/Compression.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

struct LoadedFile {
    std::vector<std::uint8_t> bytes;
    omw05::core::ByteSpan span() const {
        return omw05::core::ByteSpan(bytes.data(), bytes.size());
    }
};

bool load(const std::string& path, LoadedFile* out) {
    auto file = omw05::core::readFile(path);
    if (!file) {
        std::fprintf(stderr, "error: %s\n", file.error().message.c_str());
        return false;
    }
    out->bytes = file.take();
    auto d = omw05::io::decompressAuto(out->span());
    if (d.ok()) {
        out->bytes = d.take();
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::string geomPath;
    std::string texPath;
    const char* gamedir = std::getenv("OMW05_GAMEDIR");
    const char* car = nullptr;
    bool showMarkers = false;
    std::vector<const char*> positional;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--gamedir") && i + 1 < argc) {
            gamedir = argv[++i];
        } else if (!std::strcmp(argv[i], "--car") && i + 1 < argc) {
            car = argv[++i];
        } else if (!std::strcmp(argv[i], "--markers")) {
            showMarkers = true;
        } else {
            positional.push_back(argv[i]);
        }
    }
    if (car && gamedir) {
        geomPath = std::string(gamedir) + "/CARS/" + car + "/GEOMETRY.BIN";
        texPath = std::string(gamedir) + "/CARS/" + car + "/TEXTURES.BIN";
    } else if (positional.size() == 2) {
        geomPath = positional[0];
        texPath = positional[1];
    } else {
        std::fprintf(stderr,
                     "usage: mw-cardump --gamedir <dir> --car <NAME> [--markers]\n"
                     "       mw-cardump <geometry.bin> <textures.bin> [--markers]\n");
        return 2;
    }

    LoadedFile geom;
    LoadedFile tex;
    if (!load(geomPath, &geom) || !load(texPath, &tex)) {
        return 1;
    }

    // --- Textures ---------------------------------------------------------
    std::map<std::uint32_t, std::string> textureNames;
    std::map<std::uint32_t, bool> textureDecodable;
    auto packs = omw05::formats::parseTexturePacks(tex.span());
    if (packs) {
        for (const auto& pack : packs.value()) {
            std::printf("pack '%s' (%s): %zu textures\n", pack.name.c_str(),
                        pack.filename.c_str(), pack.textures.size());
            for (const auto& t : pack.textures) {
                const bool ok = !omw05::formats::decodeTextureRgba(t).empty();
                textureNames[t.nameHash] = t.name;
                textureDecodable[t.nameHash] = ok;
                std::printf("  0x%08X %-28s %4ux%-4u %-8s pal=%-6u %s\n", t.nameHash,
                            t.name.c_str(), t.width, t.height,
                            omw05::formats::textureFormatName(t.textureFormat()),
                            t.paletteSize, ok ? "ok" : "UNDECODED");
            }
        }
    }

    // --- Geometry ---------------------------------------------------------
    auto lists = omw05::formats::parseSolidLists(geom.span());
    if (!lists) {
        std::fprintf(stderr, "error: %s\n", lists.error().message.c_str());
        return 1;
    }
    int missing = 0;
    for (const auto& list : lists.value()) {
        for (const auto& obj : list.objects) {
            std::printf("object %-44s hash=0x%08X sets=%zu markers=%zu\n", obj.name.c_str(),
                        obj.hash, obj.vertexSets.size(), obj.markers.size());
            for (std::size_t mi = 0; mi < obj.materials.size(); ++mi) {
                const auto& mat = obj.materials[mi];
                auto it = textureNames.find(mat.diffuseTextureHash);
                const char* status;
                std::string resolved;
                if (it == textureNames.end()) {
                    status = "MISSING";
                    ++missing;
                } else if (!textureDecodable[mat.diffuseTextureHash]) {
                    status = "undecoded";
                    resolved = it->second;
                } else {
                    status = "ok";
                    resolved = it->second;
                }
                std::printf("  mat[%zu] '%s' effect=%u verts=%u idx=%zu diffuse=0x%08X %s %s\n",
                            mi, mat.name.c_str(), mat.effectId, mat.numVerts,
                            mat.indices.size(), mat.diffuseTextureHash, status,
                            resolved.c_str());
            }
            if (showMarkers) {
                for (const auto& m : obj.markers) {
                    std::printf("  marker 0x%08X i=%d f=(%.2f,%.2f) pos=(%.3f, %.3f, %.3f)\n",
                                m.nameHash, m.iParam, static_cast<double>(m.fParam0),
                                static_cast<double>(m.fParam1),
                                static_cast<double>(m.matrix[12]),
                                static_cast<double>(m.matrix[13]),
                                static_cast<double>(m.matrix[14]));
                }
            }
        }
    }
    std::printf("%d material(s) reference textures missing from this TEXTURES.BIN\n", missing);
    return 0;
}
