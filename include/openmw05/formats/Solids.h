#pragma once
// NFS:MW (2005, PC) solid geometry parser — CPU-side only (CLAUDE.md §4).
//
// Format knowledge from NFSTools/NFS-ModTools (no license file — used as
// format documentation only, code re-implemented here; credited in
// CREDITS.md): Common/Geometry/SolidReader.cs, MostWantedSolidReader.cs,
// MostWantedSolidListReader.cs. Field tables in docs/formats/solids.md.
//
// Chunk structure:
//   GeometryPack 0x80134000
//     0x80134001 header container
//       0x00134002 list info (filename/group)
//       0x00134003 object hash table, 0x00134004 (unknown, 24 b/entry)
//     0x80134010 solid object container (one per object)
//       0x00134011 object header (version 0x16) + name
//       0x00134012 texture hash table {u32 hash, u32 pad}
//       0x00134013 light materials, 0x00134017..1A smoothing/markers
//       0x80134100 mesh ("plat") container
//         0x00134900 descriptor, 0x00134b01 vertex buffer(s),
//         0x00134b02 shading groups (0x68 bytes), 0x00134b03 u16 indices,
//         0x00134c02 material names
//
// Vertices are normalized to SolidVertex (pos/normal/color/uv); tangents and
// bone weights present in some effects are skipped. Unknown effect IDs are
// zero-filled using the buffer stride and logged — never guessed.

#include "openmw05/core/Result.h"
#include "openmw05/core/Span17.h"

#include <cstdint>
#include <string>
#include <vector>

namespace omw05 {
namespace formats {

struct SolidVertex {
    float position[3] = {0, 0, 0};
    float normal[3] = {0, 0, 0};
    std::uint32_t color = 0xFFFFFFFF;  // BGRA byte order as stored
    float uv[2] = {0, 0};
};

struct SolidMaterial {
    std::string name;  // from 0x00134c02, often empty
    std::uint32_t diffuseTextureHash = 0;
    std::uint32_t normalTextureHash = 0;    // 0 = same as diffuse/absent
    std::uint32_t specularTextureHash = 0;  // 0 = same as diffuse/absent
    std::uint32_t effectId = 0;
    std::uint32_t flags = 0;
    std::uint32_t numVerts = 0;
    std::uint32_t vertexSetIndex = 0;  // which vertexSets[] entry indices refer to
    float boundsMin[3] = {0, 0, 0};
    float boundsMax[3] = {0, 0, 0};
    std::vector<std::uint16_t> indices;  // triangle list into the vertex set
};

// 0x0013401A entry (0x50 bytes; decomp ePositionMarker): attachment points
// baked into the object (wheel positions on cars, exhausts, etc.).
struct PositionMarker {
    std::uint32_t nameHash = 0;
    std::int32_t iParam = 0;
    float fParam0 = 0;
    float fParam1 = 0;
    float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};  // translation in row 3
};

struct SolidObject {
    std::string name;
    std::uint32_t hash = 0;  // binHash of name
    float boundsMin[3] = {0, 0, 0};
    float boundsMax[3] = {0, 0, 0};
    float transform[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};  // row-major 4x4
    std::vector<std::uint32_t> textureHashes;
    std::vector<SolidMaterial> materials;
    std::vector<std::vector<SolidVertex>> vertexSets;
    std::vector<PositionMarker> markers;
};

struct SolidList {
    std::string filename;   // original path baked into the bundle
    std::string groupName;
    std::vector<SolidObject> objects;
};

// Scans a chunked buffer for GeometryPack chunks. Empty result vector is
// valid for files without geometry. Corrupt objects are skipped with a log.
core::Result<std::vector<SolidList>> parseSolidLists(core::ByteSpan file);

} // namespace formats
} // namespace omw05
