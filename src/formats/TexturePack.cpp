#include "openmw05/formats/TexturePack.h"

#include "openmw05/core/Log.h"
#include "openmw05/core/Stream.h"
#include "openmw05/io/ChunkIds.h"
#include "openmw05/io/ChunkReader.h"

namespace omw05 {
namespace formats {

namespace {

constexpr std::size_t kTextureEntrySize = 0x7C;  // Nikki Texture.Disassemble (MW)
constexpr std::size_t kDataBase = 0x7C;          // Nikki TPKBlock.Disassemble (MW)

std::string readFixedString(core::Stream& s, std::size_t len) {
    core::ByteSpan raw = s.bytes(len);
    std::string out;
    for (std::size_t i = 0; i < raw.size() && raw[i] != 0; ++i) {
        out.push_back(static_cast<char>(raw[i]));
    }
    return out;
}

// TPK_InfoPart1: u32 headerSize(0x7C), u32 version, char[0x1C] name,
// char[0x40] filename, u32 key (per Nikki TPKBlock.GetHeaderInfo).
void parseInfoPart1(core::ByteSpan payload, TexturePack* pack) {
    core::Stream s(payload);
    const std::uint32_t headerSize = s.u32();
    if (headerSize != 0x7C) {
        OMW05_LOG_WARN("formats", "TPK InfoPart1 header size 0x%X (expected 0x7C)", headerSize);
        return;
    }
    s.skip(4);  // version
    pack->name = readFixedString(s, 0x1C);
    pack->filename = readFixedString(s, 0x40);
    pack->key = s.u32();
}

// TPK_InfoPart4: 0x7C bytes per texture (per Nikki Texture.Disassemble, MW):
// 0x00 +0xC skip, 0x0C char[0x18] name, 0x24 binkey, 0x28 classkey,
// 0x2C unknown, 0x30 dataOffset, 0x34 paletteOffset, 0x38 size,
// 0x3C paletteSize, 0x40 area, 0x44 u16 width, 0x46 u16 height,
// 0x48 u8 log2w, 0x49 u8 log2h, 0x4A u8 compression, 0x4B u8 palComp,
// 0x4C u16 numPalettes, 0x4E u8 mipmaps, ... (rest: render/scroll flags).
void parseTextureEntry(core::ByteSpan entry, TextureEntry* out) {
    core::Stream s(entry);
    s.skip(0xC);
    out->name = readFixedString(s, 0x18);
    out->nameHash = s.u32();
    out->classHash = s.u32();
    s.skip(4);  // unknown0
    out->dataOffset = s.u32();
    out->paletteOffset = s.u32();
    out->dataSize = s.u32();
    out->paletteSize = s.u32();
    s.skip(4);  // area
    out->width = s.u16();
    out->height = s.u16();
    s.skip(2);  // log2 width/height
    out->format = s.u8();
    s.skip(1);  // palette compression
    s.skip(2);  // num palettes
    out->mipmaps = s.u8();
}

void parseInfoBlock(core::ByteSpan payload, TexturePack* pack) {
    (void)io::forEachChunk(payload, 0, [pack](const io::Chunk& c) {
        switch (static_cast<io::ChunkId>(c.id)) {
        case io::ChunkId::TPK_InfoPart1:
            parseInfoPart1(c.data, pack);
            break;
        case io::ChunkId::TPK_InfoPart4: {
            const std::size_t count = c.data.size() / kTextureEntrySize;
            for (std::size_t i = 0; i < count; ++i) {
                TextureEntry entry;
                parseTextureEntry(c.data.subspan(i * kTextureEntrySize, kTextureEntrySize),
                                  &entry);
                pack->textures.push_back(entry);
            }
            break;
        }
        default:
            break;  // InfoPart2 (hash table) and InfoPart5 (comp slots) not needed
        }
    });
}

void attachData(core::ByteSpan dataPart2, TexturePack* pack) {
    for (TextureEntry& tex : pack->textures) {
        tex.data = dataPart2.subspan(kDataBase + tex.dataOffset, tex.dataSize);
        if (tex.data.size() != tex.dataSize) {
            OMW05_LOG_WARN("formats", "texture '%s' data truncated (%zu of %u bytes)",
                           tex.name.c_str(), tex.data.size(), tex.dataSize);
        }
        if (tex.paletteSize) {
            tex.palette = dataPart2.subspan(kDataBase + tex.paletteOffset, tex.paletteSize);
        }
    }
}

} // namespace

core::Result<std::vector<TexturePack>> parseTexturePacks(core::ByteSpan file) {
    std::vector<TexturePack> packs;
    bool awaitingData = false;

    core::Result<void> walk = io::walkChunks(file, [&](const io::Chunk& c, int) {
        switch (static_cast<io::ChunkId>(c.id)) {
        case io::ChunkId::TPK_InfoBlock:
            packs.emplace_back();
            parseInfoBlock(c.data, &packs.back());
            awaitingData = true;
            return false;
        case io::ChunkId::TPK_DataBlock: {
            if (!awaitingData || packs.empty()) {
                OMW05_LOG_WARN("formats", "TPK data block without preceding info block");
                return false;
            }
            // Find the DataPart2 child carrying the actual bytes.
            (void)io::forEachChunk(c.data, 0, [&](const io::Chunk& child) {
                if (child.id == static_cast<std::uint32_t>(io::ChunkId::TPK_DataPart2)) {
                    attachData(child.data, &packs.back());
                }
            });
            awaitingData = false;
            return false;
        }
        default:
            return true;  // keep descending (TPKBlocks wrapper, track bundles)
        }
    });
    if (!walk) {
        return walk.error();
    }
    return packs;
}

} // namespace formats
} // namespace omw05
