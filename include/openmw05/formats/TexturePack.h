#pragma once
// NFS:MW (2005, PC) texture pack (TPK) parser — CPU-side only, never touches
// GL (CLAUDE.md §4). Produces plain structs; gfx/ uploads them, tools dump.
//
// Format sources (see docs/formats/texturepacks.md and THIRD_PARTY.md):
// - Nikki (MaxHwoy, MIT): Support.MostWanted/Class/TPKBlock.cs + Texture.cs —
//   authoritative MW field layout (0x7C-byte texture entries, part chunks).
// - Chunk IDs from ChunkIds.h (TPK_* entries).
//
// Layout summary:
//   TPK_InfoBlock (0xB3310000) container:
//     TPK_InfoPart1 (0x33310001): u32 headerSize(0x7C), u32 version,
//       char[0x1C] name, char[0x40] filename, u32 key, rest unknown
//     TPK_InfoPart2 (0x33310002): {u32 hash, u32 0} per texture
//     TPK_InfoPart4 (0x33310004): 0x7C-byte texture entries (below)
//     TPK_InfoPart5 (0x33310005): per-texture compression slots (opaque here)
//   TPK_DataBlock (0xB3320000) container:
//     TPK_DataPart2 (0x33320002): texture bytes; entry offsets are relative
//       to payload start + 0x7C (per Nikki TPKBlock.Disassemble)

#include "openmw05/core/Result.h"
#include "openmw05/core/Span17.h"

#include <cstdint>
#include <string>
#include <vector>

namespace omw05 {
namespace formats {

// TextureCompressionType from Nikki (Reflection/Enum/TextureCompressionType.cs).
// Only the members observed in MW PC packs get first-class decode support;
// everything else is preserved numerically and reported unsupported.
enum class TextureFormat : std::uint8_t {
    Default = 0,
    P4 = 4,       // 4-bit palettized
    P8 = 8,       // 8-bit palettized
    Rgb16 = 16,
    Rgba16_1555 = 17,
    Rgb16_565 = 18,
    Rgba16_3555 = 19,
    Rgb24 = 24,
    Rgba32 = 32,
    Dxt = 33,
    Dxt1 = 34,
    Dxt3 = 36,
    Dxt5 = 38,
    DxtN = 39,
    L8 = 40,
};

struct TextureEntry {
    std::string name;            // trimmed entry name (<= 0x18 chars)
    std::uint32_t nameHash = 0;  // binHash of the real name
    std::uint32_t classHash = 0;
    std::uint32_t dataOffset = 0;     // relative to the pack's data base
    std::uint32_t paletteOffset = 0;  // ditto (palettized formats)
    std::uint32_t dataSize = 0;
    std::uint32_t paletteSize = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t mipmaps = 0;
    std::uint8_t format = 0;  // raw TextureFormat byte

    // Resolved views into the source file buffer (valid while it lives).
    core::ByteSpan data;
    core::ByteSpan palette;

    TextureFormat textureFormat() const { return static_cast<TextureFormat>(format); }
};

struct TexturePack {
    std::string name;      // InfoPart1 collection name
    std::string filename;  // original .tpk path baked into the bundle
    std::uint32_t key = 0; // binHash key of the pack
    std::vector<TextureEntry> textures;
};

// Scans a chunked buffer (whole .BUN/.BIN file or any subrange) for texture
// packs. Returns every pack found; an empty vector is a valid result for a
// file with no TPK chunks. The returned spans alias `file`.
core::Result<std::vector<TexturePack>> parseTexturePacks(core::ByteSpan file);

} // namespace formats
} // namespace omw05
