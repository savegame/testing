#include "openmw05/formats/TexturePack.h"

#include "openmw05/core/Log.h"
#include "openmw05/core/Stream.h"
#include "openmw05/io/ChunkIds.h"
#include "openmw05/io/ChunkReader.h"
#include "openmw05/io/Compression.h"

#include <algorithm>

// Layouts per Nikki (MaxHwoy, MIT): Support.MostWanted/Class/TPKBlock.cs,
// Texture.cs, Support.Shared/Class/TPKBlock.cs (ParseCompTextures),
// Support.Shared/Parts/TPKParts/OffSlot.cs + MagicHeader.cs.
// See docs/formats/texturepacks.md and THIRD_PARTY.md.

namespace omw05 {
namespace formats {

namespace {

constexpr std::size_t kTextureEntrySize = 0x7C;  // Nikki Texture.Disassemble (MW)
constexpr std::size_t kDataBase = 0x7C;          // Nikki TPKBlock.Disassemble (MW)
constexpr std::size_t kCompTexTrailer = 0x9C;    // Nikki MW CompTexHeaderSize
constexpr std::uint32_t kLzMagic = 0x55441122;   // BinBlockID.LZCompressed

// One InfoPart3 record (OffSlot.cs), 0x18 bytes.
struct OffSlot {
    std::uint32_t key = 0;
    std::uint32_t absoluteOffset = 0;  // from the InfoBlock chunk header
    std::uint32_t encodedSize = 0;
    std::uint32_t decodedSize = 0;
    std::uint8_t flags = 0;  // 0 raw, 1 one compressed stream, 2 LZC chain
};

std::string readFixedString(core::Stream& s, std::size_t len) {
    core::ByteSpan raw = s.bytes(len);
    std::string out;
    for (std::size_t i = 0; i < raw.size() && raw[i] != 0; ++i) {
        out.push_back(static_cast<char>(raw[i]));
    }
    return out;
}

// TPK_InfoPart1 payload (0x7C bytes): u32 version, char[0x1C] name,
// char[0x40] filename, u32 key (per Nikki TPKBlock.GetHeaderInfo, which
// checks the CHUNK size == 0x7C then skips the version field).
void parseInfoPart1(core::ByteSpan payload, TexturePack* pack) {
    if (payload.size() < 0x64) {
        OMW05_LOG_WARN("formats", "TPK InfoPart1 too small (%zu bytes)", payload.size());
        return;
    }
    core::Stream s(payload);
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

struct InfoBlockData {
    std::vector<OffSlot> offSlots;
};

void parseInfoBlock(core::ByteSpan payload, TexturePack* pack, InfoBlockData* info) {
    (void)io::forEachChunk(payload, 0, [pack, info](const io::Chunk& c) {
        switch (static_cast<io::ChunkId>(c.id)) {
        case io::ChunkId::TPK_InfoPart1:
            parseInfoPart1(c.data, pack);
            break;
        case io::ChunkId::TPK_InfoPart3: {  // offset slots (compressed packs)
            core::Stream s(c.data);
            const std::size_t count = c.data.size() / 0x18;
            for (std::size_t i = 0; i < count; ++i) {
                OffSlot slot;
                slot.key = s.u32();
                slot.absoluteOffset = s.u32();
                slot.encodedSize = s.u32();
                slot.decodedSize = s.u32();
                s.skip(1);  // user flags
                slot.flags = s.u8();
                s.skip(2 + 4);  // ref count, unknown
                info->offSlots.push_back(slot);
            }
            break;
        }
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
        if (tex.data.size()) {
            continue;  // already resolved (compressed path)
        }
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

// Decompresses one LZCompressed (0x55441122) block chain (offslot flags 2).
// Block layout (Nikki MagicHeader.cs): u32 magic, i32 decodedSize,
// i32 encodedSize (whole block incl. header), i32 decodedDataPosition,
// i32 encodedDataPosition, 8 pad; the compressed stream starts at +0x18.
core::Result<std::vector<std::uint8_t>> decompressLzChain(core::ByteSpan region) {
    struct Segment {
        std::uint32_t decodedPos;
        std::vector<std::uint8_t> data;
    };
    std::vector<Segment> segments;
    std::size_t pos = 0;
    while (pos + 0x18 <= region.size()) {
        if (core::readLe32(region.data() + pos) != kLzMagic) {
            pos += 4;
            continue;
        }
        core::Stream s(region.subspan(pos));
        s.skip(4);
        s.skip(4);  // decoded size (trust the stream's own header instead)
        const std::uint32_t encodedSize = s.u32();
        const std::uint32_t decodedPos = s.u32();
        if (encodedSize < 0x18 || pos + encodedSize > region.size()) {
            return core::Error{core::ErrorCode::CorruptData, "LZC block overruns region"};
        }
        auto blob = io::decompressAuto(region.subspan(pos + 0x18, encodedSize - 0x18));
        if (!blob) {
            return blob.error();
        }
        segments.push_back({decodedPos, blob.take()});
        pos += encodedSize;
    }
    if (segments.empty()) {
        return core::Error{core::ErrorCode::CorruptData, "no LZC blocks in chain"};
    }
    std::size_t total = 0;
    for (const Segment& seg : segments) {
        total += seg.data.size();
    }
    std::sort(segments.begin(), segments.end(),
              [](const Segment& a, const Segment& b) { return a.decodedPos < b.decodedPos; });
    std::vector<std::uint8_t> out;
    out.reserve(total);
    for (const Segment& seg : segments) {
        out.insert(out.end(), seg.data.begin(), seg.data.end());
    }
    return out;
}

// Compressed-pack path (Nikki ParseCompTextures): each offslot yields one
// texture; the decompressed blob ends with a 0x9C trailer whose first 0x7C
// bytes are the standard texture entry.
void parseCompressedTextures(core::ByteSpan file, std::size_t infoBlockOffset,
                             const std::vector<OffSlot>& slots, TexturePack* pack) {
    for (const OffSlot& slot : slots) {
        core::ByteSpan region = file.subspan(infoBlockOffset + slot.absoluteOffset,
                                             slot.encodedSize);
        if (region.size() != slot.encodedSize) {
            OMW05_LOG_WARN("formats", "TPK offslot 0x%08X out of file bounds", slot.key);
            continue;
        }

        core::Result<std::vector<std::uint8_t>> blob = [&]() {
            switch (slot.flags) {
            case 0:
                return core::Result<std::vector<std::uint8_t>>(
                    std::vector<std::uint8_t>(region.begin(), region.end()));
            case 1:
                return io::decompressAuto(region);
            case 2:
                return decompressLzChain(region);
            default:
                return core::Result<std::vector<std::uint8_t>>(core::Error{
                    core::ErrorCode::UnsupportedData,
                    "unknown offslot flags " + std::to_string(slot.flags)});
            }
        }();
        if (!blob) {
            OMW05_LOG_WARN("formats", "TPK offslot 0x%08X: %s", slot.key,
                           blob.error().message.c_str());
            continue;
        }
        std::vector<std::uint8_t> bytes = blob.take();
        if (bytes.size() < kCompTexTrailer) {
            OMW05_LOG_WARN("formats", "TPK offslot 0x%08X: blob smaller than trailer",
                           slot.key);
            continue;
        }

        TextureEntry entry;
        parseTextureEntry(core::ByteSpan(bytes.data() + bytes.size() - kCompTexTrailer,
                                         kTextureEntrySize),
                          &entry);

        // Data layout inside the blob (Nikki ParseCompTextures): pixel data
        // starts at 0 when there is no palette; otherwise the palette/data
        // offsets are file-relative and only their delta matters.
        std::size_t palOff = 0;
        std::size_t datOff = 0;
        if (entry.paletteSize != 0) {
            if (entry.paletteOffset > entry.paletteSize &&
                entry.paletteOffset >= entry.dataOffset) {
                palOff = entry.paletteOffset - entry.dataOffset;
            } else if (entry.dataOffset >= entry.paletteOffset) {
                datOff = entry.dataOffset - entry.paletteOffset;
            }
        }
        if (datOff + entry.dataSize > bytes.size() ||
            (entry.paletteSize && palOff + entry.paletteSize > bytes.size())) {
            OMW05_LOG_WARN("formats", "TPK texture '%s': compressed blob too small",
                           entry.name.c_str());
            continue;
        }

        pack->decompressed.push_back(std::move(bytes));
        const std::vector<std::uint8_t>& owned = pack->decompressed.back();
        entry.data = core::ByteSpan(owned.data() + datOff, entry.dataSize);
        if (entry.paletteSize) {
            entry.palette = core::ByteSpan(owned.data() + palOff, entry.paletteSize);
        }
        pack->textures.push_back(std::move(entry));
    }
}

} // namespace

core::Result<std::vector<TexturePack>> parseTexturePacks(core::ByteSpan file) {
    std::vector<TexturePack> packs;
    bool awaitingData = false;
    // Compressed-pack offsets are relative to the enclosing TPKBlocks
    // (0xB3300000) wrapper header (Nikki positions the reader there before
    // TPKBlock.Disassemble). Standalone InfoBlocks fall back to their own
    // offset.
    std::size_t wrapperOffset = 0;
    std::size_t wrapperEnd = 0;

    core::Result<void> walk = io::walkChunks(file, [&](const io::Chunk& c, int) {
        switch (static_cast<io::ChunkId>(c.id)) {
        case io::ChunkId::TPKBlocks:
            wrapperOffset = c.fileOffset;
            wrapperEnd = c.fileOffset + 8 + c.data.size();
            return true;
        case io::ChunkId::TPK_InfoBlock: {
            packs.emplace_back();
            InfoBlockData info;
            parseInfoBlock(c.data, &packs.back(), &info);
            if (!info.offSlots.empty()) {
                const bool wrapped =
                    c.fileOffset >= wrapperOffset && c.fileOffset < wrapperEnd;
                parseCompressedTextures(file, wrapped ? wrapperOffset : c.fileOffset,
                                        info.offSlots, &packs.back());
                awaitingData = false;
            } else {
                awaitingData = true;
            }
            return false;
        }
        case io::ChunkId::TPK_DataBlock: {
            if (!awaitingData || packs.empty()) {
                return false;  // compressed pack: bytes already consumed via offslots
            }
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
