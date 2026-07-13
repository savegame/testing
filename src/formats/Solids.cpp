#include "openmw05/formats/Solids.h"

#include "openmw05/core/Log.h"
#include "openmw05/core/Stream.h"
#include "openmw05/io/ChunkIds.h"
#include "openmw05/io/ChunkReader.h"

// Implementation follows NFSTools/NFS-ModTools (Common/Geometry/
// SolidReader.cs, MostWantedSolidReader.cs, MostWantedSolidListReader.cs);
// see docs/formats/solids.md for the field tables and THIRD_PARTY.md /
// CREDITS.md for provenance.

namespace omw05 {
namespace formats {

namespace {

// Blackbox aligns several payloads to absolute file offsets; the pad bytes
// live inside the chunk payload. Returns the number of bytes to skip.
std::size_t alignPad(std::size_t absOffset, std::size_t alignment) {
    const std::size_t rem = absOffset % alignment;
    return rem == 0 ? 0 : alignment - rem;
}

std::string readCString(core::Stream& s) {
    std::string out;
    while (s.remaining() > 0) {
        const char c = static_cast<char>(s.u8());
        if (c == 0) {
            break;
        }
        out.push_back(c);
    }
    return out;
}

std::string readFixedString(core::Stream& s, std::size_t len) {
    core::ByteSpan raw = s.bytes(len);
    std::string out;
    for (std::size_t i = 0; i < raw.size() && raw[i] != 0; ++i) {
        out.push_back(static_cast<char>(raw[i]));
    }
    return out;
}

// InternalEffectId order per NFS-ModTools MostWantedSolidReader.
enum Effect : std::uint32_t {
    WorldShader = 0,
    WorldReflectShader = 1,
    WorldBoneShader = 2,
    WorldNormalMap = 3,
    CarShader = 4,
    GlossyWindow = 5,
    BillboardShader = 6,
    SkyShader = 19,
};

// Reads one vertex according to the material's effect ID. Returns false for
// effects with unknown layouts (caller zero-fills and advances by stride).
bool readVertex(core::Stream& s, std::uint32_t effectId, SolidVertex* v) {
    switch (effectId) {
    case WorldShader:
    case CarShader:
    case GlossyWindow:
    case BillboardShader:  // pos, normal, color, uv (stride 36)
        break;
    case WorldNormalMap:
    case WorldReflectShader:  // + 8 unknown, tangent(12), pad 4 (stride 60)
    case SkyShader:           // + 8 unknown (stride 44)
    case WorldBoneShader:     // + blend weights/indices (stride 60)
        break;
    default:
        return false;
    }
    v->position[0] = s.f32();
    v->position[1] = s.f32();
    v->position[2] = s.f32();
    v->normal[0] = s.f32();
    v->normal[1] = s.f32();
    v->normal[2] = s.f32();
    v->color = s.u32();
    v->uv[0] = s.f32();
    v->uv[1] = s.f32();
    switch (effectId) {
    case WorldNormalMap:
    case WorldReflectShader:
        s.skip(8 + 12 + 4);
        break;
    case SkyShader:
        s.skip(8);
        break;
    case WorldBoneShader:
        s.skip(24);
        break;
    default:
        break;
    }
    return true;
}

// --- Solid-level chunk parsers -------------------------------------------

// 0x00134011: align 0x10, 0xA0-byte header, then null-terminated name.
void parseObjectHeader(const io::Chunk& c, SolidObject* obj) {
    core::Stream s(c.data);
    s.skip(alignPad(c.fileOffset + 8, 0x10));
    s.skip(12);  // blank
    const std::uint8_t version = s.u8();
    if (version != 0x16) {
        OMW05_LOG_WARN("formats", "solid header version 0x%X (expected 0x16)", version);
    }
    s.skip(1 + 2);  // endian-swapped flag, flags
    obj->hash = s.u32();
    s.skip(2 + 2);      // numPolys, numVerts
    s.skip(4);          // numBones, numTextureTableEntries, numLightMats, numMarkers
    s.skip(4);          // blank
    for (int i = 0; i < 3; ++i) {
        obj->boundsMin[i] = s.f32();
    }
    s.skip(4);
    for (int i = 0; i < 3; ++i) {
        obj->boundsMax[i] = s.f32();
    }
    s.skip(4);
    for (int i = 0; i < 16; ++i) {
        obj->transform[i] = s.f32();
    }
    s.skip(8 + 4 + 4 + 4 + 4 + 4 + 4);  // trailing unknowns
    obj->name = readCString(s);
}

// 0x00134012: {u32 hash, u32 pad} per referenced texture.
void parseTextureTable(const io::Chunk& c, SolidObject* obj) {
    core::Stream s(c.data);
    const std::size_t count = c.data.size() / 8;
    for (std::size_t i = 0; i < count; ++i) {
        obj->textureHashes.push_back(s.u32());
        s.skip(4);
    }
}

// 0x00134b02: 0x68-byte shading group entries (offsets per NFS-ModTools).
void parseShadingGroups(const io::Chunk& c, SolidObject* obj, std::uint32_t* totalVerts) {
    core::Stream s(c.data);
    s.skip(alignPad(c.fileOffset + 8, 0x10));
    const std::size_t kEntry = 0x68;
    const std::size_t count = s.remaining() / kEntry;
    std::uint32_t lastEffectId = 0;
    std::uint32_t streamIndex = 0;

    for (std::size_t j = 0; j < count; ++j) {
        SolidMaterial mat;
        for (int i = 0; i < 3; ++i) {
            mat.boundsMin[i] = s.f32();
        }
        for (int i = 0; i < 3; ++i) {
            mat.boundsMax[i] = s.f32();
        }
        const std::uint8_t diffuseId = s.u8();
        const std::uint8_t normalId = s.u8();
        s.skip(1);  // height map id
        const std::uint8_t specularId = s.u8();
        s.skip(1);  // opacity map id
        s.skip(1);  // light material number
        s.skip(2);  // unknown i16
        s.skip(0x10);
        mat.effectId = s.u32();
        s.skip(4);  // effect pointer (always 0 on disk)
        mat.flags = s.u32();
        mat.numVerts = s.u32();
        const std::uint32_t numTris = s.u32();
        s.skip(0x18);
        s.skip(4);  // numIndices field (equals numTris*3)
        s.skip(8);

        auto hashAt = [obj](std::uint8_t idx) -> std::uint32_t {
            return idx < obj->textureHashes.size() ? obj->textureHashes[idx] : 0;
        };
        mat.diffuseTextureHash = hashAt(diffuseId);
        mat.normalTextureHash = normalId == diffuseId ? 0 : hashAt(normalId);
        mat.specularTextureHash = specularId == diffuseId ? 0 : hashAt(specularId);
        mat.indices.resize(static_cast<std::size_t>(numTris) * 3);  // filled by 0x134b03

        if (j > 0 && mat.effectId != lastEffectId) {
            ++streamIndex;
        }
        mat.vertexSetIndex = streamIndex;
        lastEffectId = mat.effectId;
        *totalVerts += mat.numVerts;
        obj->materials.push_back(std::move(mat));
    }
    if (!s.ok()) {
        OMW05_LOG_WARN("formats", "solid '%s': truncated shading groups", obj->name.c_str());
    }
}

// 0x00134b03: align 0x10, u16 indices for every material sequentially.
void parseIndices(const io::Chunk& c, SolidObject* obj) {
    core::Stream s(c.data);
    s.skip(alignPad(c.fileOffset + 8, 0x10));
    for (SolidMaterial& mat : obj->materials) {
        for (std::uint16_t& idx : mat.indices) {
            idx = s.u16();
        }
    }
    if (!s.ok()) {
        OMW05_LOG_WARN("formats", "solid '%s': truncated index buffer", obj->name.c_str());
        for (SolidMaterial& mat : obj->materials) {
            mat.indices.clear();
        }
    }
}

// Distributes raw vertex buffers into decoded per-stream vertex sets
// (mirrors NFS-ModTools SolidReader.PostProcessSolid).
void buildVertexSets(SolidObject* obj, const std::vector<core::ByteSpan>& buffers,
                     std::uint32_t totalVerts) {
    if (buffers.empty()) {
        return;
    }
    std::vector<std::uint32_t> counts(buffers.size(), 0);
    if (buffers.size() == 1) {
        counts[0] = totalVerts;
    } else {
        for (const SolidMaterial& m : obj->materials) {
            if (m.vertexSetIndex < counts.size()) {
                counts[m.vertexSetIndex] += m.numVerts;
            }
        }
    }

    obj->vertexSets.resize(buffers.size());
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        if (counts[i] == 0 || buffers[i].size() % counts[i] != 0) {
            if (counts[i] != 0) {
                OMW05_LOG_WARN("formats", "solid '%s': stream %zu size %zu not divisible by %u",
                               obj->name.c_str(), i, buffers[i].size(), counts[i]);
                counts[i] = 0;
            }
        }
        obj->vertexSets[i].resize(counts[i]);
    }

    std::vector<std::uint32_t> consumed(buffers.size(), 0);
    for (const SolidMaterial& mat : obj->materials) {
        const std::uint32_t setIdx = mat.vertexSetIndex;
        if (setIdx >= buffers.size() || counts[setIdx] == 0) {
            continue;
        }
        const std::size_t stride = buffers[setIdx].size() / counts[setIdx];
        const std::uint32_t numVerts = mat.numVerts == 0 ? counts[setIdx] : mat.numVerts;
        if (consumed[setIdx] >= counts[setIdx]) {
            continue;  // stream fully consumed (shared by earlier material)
        }
        for (std::uint32_t i = 0; i < numVerts && consumed[setIdx] + i < counts[setIdx]; ++i) {
            const std::size_t base = static_cast<std::size_t>(consumed[setIdx] + i) * stride;
            core::Stream vs(buffers[setIdx].subspan(base, stride));
            SolidVertex v;
            if (!readVertex(vs, mat.effectId, &v)) {
                OMW05_LOG_DEBUG("formats", "solid '%s': unknown effect %u, zero vertices",
                                obj->name.c_str(), mat.effectId);
            }
            obj->vertexSets[setIdx][consumed[setIdx] + i] = v;
        }
        consumed[setIdx] += numVerts;
    }
}

// 0x80134010 payload: solid chunks + nested 0x80134100 mesh container.
SolidObject parseSolidObject(const io::Chunk& container) {
    SolidObject obj;
    std::uint32_t totalVerts = 0;
    std::vector<core::ByteSpan> vertexBuffers;
    std::size_t namedMaterials = 0;

    (void)io::forEachChunk(container.data, container.fileOffset + 8, [&](const io::Chunk& c) {
        switch (c.id) {
        case 0x00134011:
            parseObjectHeader(c, &obj);
            break;
        case 0x00134012:
            parseTextureTable(c, &obj);
            break;
        case 0x0013401A: {  // position markers, 0x50 each (ePositionMarker)
            core::Stream ms(c.data.subspan(alignPad(c.fileOffset + 8, 0x10)));
            const std::size_t count = ms.remaining() / 0x50;
            for (std::size_t i = 0; i < count; ++i) {
                PositionMarker marker;
                marker.nameHash = ms.u32();
                marker.iParam = ms.i32();
                marker.fParam0 = ms.f32();
                marker.fParam1 = ms.f32();
                for (int j = 0; j < 16; ++j) {
                    marker.matrix[j] = ms.f32();
                }
                obj.markers.push_back(marker);
            }
            break;
        }
        case 0x80134100:  // mesh ("plat") container
            (void)io::forEachChunk(c.data, c.fileOffset + 8, [&](const io::Chunk& mc) {
                switch (mc.id) {
                case 0x00134900:  // descriptor — counts derivable elsewhere
                    break;
                case 0x00134b01: {  // vertex buffer, aligned 0x80
                    const std::size_t pad = alignPad(mc.fileOffset + 8, 0x80);
                    vertexBuffers.push_back(mc.data.subspan(pad));
                    break;
                }
                case 0x00134b02:
                    parseShadingGroups(mc, &obj, &totalVerts);
                    break;
                case 0x00134b03:
                    parseIndices(mc, &obj);
                    break;
                case 0x00134c02: {  // material name
                    if (mc.data.size() > 0 && namedMaterials < obj.materials.size()) {
                        core::Stream ns(mc.data);
                        obj.materials[namedMaterials++].name = readCString(ns);
                    }
                    break;
                }
                default:
                    break;
                }
            });
            break;
        default:
            break;  // light materials, smoothing, markers: not needed yet
        }
    });

    buildVertexSets(&obj, vertexBuffers, totalVerts);
    return obj;
}

// 0x00134002 list info: i64 blank, i32 marker, i32 numObjects,
// char[0x38] filename, char[0x20] group name.
void parseListInfo(const io::Chunk& c, SolidList* list) {
    core::Stream s(c.data);
    s.skip(8 + 4 + 4);
    list->filename = readFixedString(s, 0x38);
    list->groupName = readFixedString(s, 0x20);
}

} // namespace

core::Result<std::vector<SolidList>> parseSolidLists(core::ByteSpan file) {
    std::vector<SolidList> lists;

    core::Result<void> walk = io::walkChunks(file, [&](const io::Chunk& c, int) {
        switch (static_cast<io::ChunkId>(c.id)) {
        case io::ChunkId::GeometryPack: {
            lists.emplace_back();
            SolidList& list = lists.back();
            (void)io::forEachChunk(c.data, c.fileOffset + 8, [&](const io::Chunk& top) {
                if (top.id == 0x80134001) {  // header container
                    (void)io::forEachChunk(top.data, top.fileOffset + 8, [&](const io::Chunk& h) {
                        if (h.id == 0x00134002) {
                            parseListInfo(h, &list);
                        }
                    });
                } else if (top.id == 0x80134010) {  // one solid object
                    list.objects.push_back(parseSolidObject(top));
                }
            });
            return false;  // handled this subtree
        }
        default:
            return true;
        }
    });
    if (!walk) {
        return walk.error();
    }
    return lists;
}

} // namespace formats
} // namespace omw05
