// mw-solid2gltf: exports MW solid geometry to glTF 2.0 (single .gltf with an
// embedded base64 buffer) so correctness is visually verifiable in any glTF
// viewer before the in-engine renderer exists. M3 deliverable (CLAUDE.md §5).
//
// Usage: mw-solid2gltf <file> [--out model.gltf] [--object NAME] [--decompress]
//
// Coordinates: MW data is Z-up; glTF is Y-up. Vertices are swizzled
// (x, y, z) -> (x, z, -y) on export.

#include "openmw05/core/Stream.h"
#include "openmw05/formats/Solids.h"
#include "openmw05/io/Compression.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::string base64(const std::vector<std::uint8_t>& data) {
    static const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        const std::uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(alphabet[(n >> 18) & 63]);
        out.push_back(alphabet[(n >> 12) & 63]);
        out.push_back(alphabet[(n >> 6) & 63]);
        out.push_back(alphabet[n & 63]);
    }
    if (i + 1 == data.size()) {
        const std::uint32_t n = data[i] << 16;
        out.push_back(alphabet[(n >> 18) & 63]);
        out.push_back(alphabet[(n >> 12) & 63]);
        out += "==";
    } else if (i + 2 == data.size()) {
        const std::uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        out.push_back(alphabet[(n >> 18) & 63]);
        out.push_back(alphabet[(n >> 12) & 63]);
        out.push_back(alphabet[(n >> 6) & 63]);
        out += "=";
    }
    return out;
}

void putF32(std::vector<std::uint8_t>& v, float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, 4);
    for (int i = 0; i < 4; ++i) {
        v.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
    }
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (static_cast<unsigned char>(c) >= 0x20) {
            out.push_back(c);
        }
    }
    return out;
}

struct GltfWriter {
    std::vector<std::uint8_t> bin;
    std::string bufferViews;
    std::string accessors;
    std::string meshes;
    std::string nodes;
    std::string materials;
    int numViews = 0;
    int numAccessors = 0;
    int numMeshes = 0;
    int numNodes = 0;
    int numMaterials = 0;

    int addView(const std::vector<std::uint8_t>& data, int target) {
        while (bin.size() % 4) {
            bin.push_back(0);
        }
        char buf[160];
        std::snprintf(buf, sizeof buf,
                      "%s{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu,\"target\":%d}",
                      numViews ? "," : "", bin.size(), data.size(), target);
        bufferViews += buf;
        bin.insert(bin.end(), data.begin(), data.end());
        return numViews++;
    }

    int addAccessor(int view, int componentType, const char* type, std::size_t count,
                    const float* minV = nullptr, const float* maxV = nullptr) {
        char buf[320];
        std::string mm;
        if (minV && maxV) {
            char b2[160];
            std::snprintf(b2, sizeof b2,
                          ",\"min\":[%.9g,%.9g,%.9g],\"max\":[%.9g,%.9g,%.9g]",
                          static_cast<double>(minV[0]), static_cast<double>(minV[1]),
                          static_cast<double>(minV[2]), static_cast<double>(maxV[0]),
                          static_cast<double>(maxV[1]), static_cast<double>(maxV[2]));
            mm = b2;
        }
        std::snprintf(buf, sizeof buf,
                      "%s{\"bufferView\":%d,\"componentType\":%d,\"type\":\"%s\",\"count\":%zu%s}",
                      numAccessors ? "," : "", view, componentType, type, count, mm.c_str());
        accessors += buf;
        return numAccessors++;
    }
};

} // namespace

int main(int argc, char** argv) {
    const char* path = nullptr;
    const char* outPath = "model.gltf";
    const char* onlyObject = nullptr;
    bool tryDecompress = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--out") && i + 1 < argc) {
            outPath = argv[++i];
        } else if (!std::strcmp(argv[i], "--object") && i + 1 < argc) {
            onlyObject = argv[++i];
        } else if (!std::strcmp(argv[i], "--decompress")) {
            tryDecompress = true;
        } else if (!path) {
            path = argv[i];
        }
    }
    if (!path) {
        std::fprintf(stderr,
                     "usage: mw-solid2gltf <file> [--out model.gltf] [--object NAME] "
                     "[--decompress]\n");
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

    auto listsResult = omw05::formats::parseSolidLists(span);
    if (!listsResult) {
        std::fprintf(stderr, "error: %s\n", listsResult.error().message.c_str());
        return 1;
    }

    GltfWriter g;
    int exportedObjects = 0;
    for (const auto& list : listsResult.value()) {
        for (const auto& obj : list.objects) {
            if (onlyObject && obj.name != onlyObject) {
                continue;
            }
            if (obj.vertexSets.empty()) {
                continue;
            }

            // Per vertex set: swizzled POSITION/NORMAL/TEXCOORD_0 accessors.
            std::vector<int> posAcc(obj.vertexSets.size(), -1);
            std::vector<int> nrmAcc(obj.vertexSets.size(), -1);
            std::vector<int> uvAcc(obj.vertexSets.size(), -1);
            for (std::size_t s = 0; s < obj.vertexSets.size(); ++s) {
                const auto& set = obj.vertexSets[s];
                if (set.empty()) {
                    continue;
                }
                std::vector<std::uint8_t> pos;
                std::vector<std::uint8_t> nrm;
                std::vector<std::uint8_t> uv;
                float mn[3] = {1e30f, 1e30f, 1e30f};
                float mx[3] = {-1e30f, -1e30f, -1e30f};
                for (const auto& v : set) {
                    const float p[3] = {v.position[0], v.position[2], -v.position[1]};
                    const float n[3] = {v.normal[0], v.normal[2], -v.normal[1]};
                    for (int k = 0; k < 3; ++k) {
                        putF32(pos, p[k]);
                        putF32(nrm, n[k]);
                        mn[k] = std::min(mn[k], p[k]);
                        mx[k] = std::max(mx[k], p[k]);
                    }
                    putF32(uv, v.uv[0]);
                    putF32(uv, v.uv[1]);
                }
                posAcc[s] = g.addAccessor(g.addView(pos, 34962), 5126, "VEC3", set.size(), mn, mx);
                nrmAcc[s] = g.addAccessor(g.addView(nrm, 34962), 5126, "VEC3", set.size());
                uvAcc[s] = g.addAccessor(g.addView(uv, 34962), 5126, "VEC2", set.size());
            }

            std::string primitives;
            bool first = true;
            for (const auto& mat : obj.materials) {
                if (mat.indices.empty() || mat.vertexSetIndex >= obj.vertexSets.size() ||
                    posAcc[mat.vertexSetIndex] < 0) {
                    continue;
                }
                std::vector<std::uint8_t> idx;
                idx.reserve(mat.indices.size() * 2);
                for (std::uint16_t i : mat.indices) {
                    idx.push_back(static_cast<std::uint8_t>(i));
                    idx.push_back(static_cast<std::uint8_t>(i >> 8));
                }
                const int idxAcc = g.addAccessor(g.addView(idx, 34963), 5123, "SCALAR",
                                                 mat.indices.size());
                char mbuf[160];
                std::snprintf(mbuf, sizeof mbuf, "%s{\"name\":\"%s\",\"doubleSided\":true}",
                              g.numMaterials ? "," : "",
                              jsonEscape(mat.name.empty() ? "mat" : mat.name).c_str());
                g.materials += mbuf;
                const int matIdx = g.numMaterials++;

                char buf[256];
                std::snprintf(
                    buf, sizeof buf,
                    "%s{\"attributes\":{\"POSITION\":%d,\"NORMAL\":%d,\"TEXCOORD_0\":%d},"
                    "\"indices\":%d,\"material\":%d}",
                    first ? "" : ",", posAcc[mat.vertexSetIndex], nrmAcc[mat.vertexSetIndex],
                    uvAcc[mat.vertexSetIndex], idxAcc, matIdx);
                primitives += buf;
                first = false;
            }
            if (primitives.empty()) {
                continue;
            }

            char buf[256];
            std::snprintf(buf, sizeof buf, "%s{\"name\":\"%s\",\"primitives\":[%s]}",
                          g.numMeshes ? "," : "", jsonEscape(obj.name).c_str(),
                          primitives.c_str());
            g.meshes += buf;
            std::snprintf(buf, sizeof buf, "%s{\"name\":\"%s\",\"mesh\":%d}",
                          g.numNodes ? "," : "", jsonEscape(obj.name).c_str(), g.numMeshes);
            g.nodes += buf;
            ++g.numMeshes;
            ++g.numNodes;
            ++exportedObjects;
            std::printf("exported '%s': %zu materials, %zu vertex set(s)\n", obj.name.c_str(),
                        obj.materials.size(), obj.vertexSets.size());
        }
    }

    if (!exportedObjects) {
        std::fprintf(stderr, "no exportable objects found\n");
        return 1;
    }

    std::string nodeIndices;
    for (int i = 0; i < g.numNodes; ++i) {
        nodeIndices += (i ? "," : "") + std::to_string(i);
    }

    std::FILE* out = std::fopen(outPath, "wb");
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", outPath);
        return 1;
    }
    std::fprintf(out,
                 "{\"asset\":{\"version\":\"2.0\",\"generator\":\"omw05 mw-solid2gltf\"},"
                 "\"scene\":0,\"scenes\":[{\"nodes\":[%s]}],"
                 "\"nodes\":[%s],\"meshes\":[%s],\"materials\":[%s],"
                 "\"bufferViews\":[%s],\"accessors\":[%s],"
                 "\"buffers\":[{\"byteLength\":%zu,\"uri\":\"data:application/octet-stream;"
                 "base64,%s\"}]}",
                 nodeIndices.c_str(), g.nodes.c_str(), g.meshes.c_str(), g.materials.c_str(),
                 g.bufferViews.c_str(), g.accessors.c_str(), g.bin.size(),
                 base64(g.bin).c_str());
    std::fclose(out);
    std::printf("wrote %s (%d object(s), %zu-byte buffer)\n", outPath, exportedObjects,
                g.bin.size());
    return 0;
}
