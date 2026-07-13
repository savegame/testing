// TexturePack parser + DXT decoder tests on synthetic buffers (CLAUDE.md §4).
#include <doctest.h>

#include "openmw05/formats/TexturePack.h"
#include "openmw05/gfx/DxtDecode.h"

#include <cstring>
#include <string>
#include <vector>

using namespace omw05;

namespace {

void putLe32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) {
        v.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
    }
}
void putLe16(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
}
void putFixedString(std::vector<std::uint8_t>& v, const char* s, std::size_t len) {
    std::size_t i = 0;
    for (; s[i] && i < len; ++i) {
        v.push_back(static_cast<std::uint8_t>(s[i]));
    }
    for (; i < len; ++i) {
        v.push_back(0);
    }
}

// Builds a minimal MW-layout TPK with one 4x4 DXT1 texture (layout per
// docs/formats/texturepacks.md / Nikki).
std::vector<std::uint8_t> buildSyntheticTpk(const std::vector<std::uint8_t>& dxtBlock) {
    // InfoPart1 (0x7C bytes): u32 version, name, filename, key
    std::vector<std::uint8_t> part1;
    putLe32(part1, 8);  // version
    putFixedString(part1, "TESTPACK", 0x1C);
    putFixedString(part1, "tex\\testpack.tpk", 0x40);
    putLe32(part1, 0xCAFEBABE);  // key
    while (part1.size() < 0x7C) {
        part1.push_back(0);
    }

    // InfoPart4: one 0x7C entry
    std::vector<std::uint8_t> entry;
    for (int i = 0; i < 0xC; ++i) {
        entry.push_back(0);
    }
    putFixedString(entry, "TESTTEX", 0x18);
    putLe32(entry, 0x12345678);  // binkey
    putLe32(entry, 0x0);         // classkey
    putLe32(entry, 0);           // unknown0
    putLe32(entry, 0);           // dataOffset
    putLe32(entry, 0);           // paletteOffset
    putLe32(entry, static_cast<std::uint32_t>(dxtBlock.size()));  // size
    putLe32(entry, 0);           // paletteSize
    putLe32(entry, 16);          // area
    putLe16(entry, 4);           // width
    putLe16(entry, 4);           // height
    entry.push_back(2);          // log2 w
    entry.push_back(2);          // log2 h
    entry.push_back(34);         // compression = TEXCOMP_DXTC1
    entry.push_back(0);          // pal comp
    putLe16(entry, 0);           // num palettes
    entry.push_back(1);          // mipmaps
    while (entry.size() < 0x7C) {
        entry.push_back(0);
    }

    std::vector<std::uint8_t> info;
    putLe32(info, 0x33310001);
    putLe32(info, static_cast<std::uint32_t>(part1.size()));
    info.insert(info.end(), part1.begin(), part1.end());
    putLe32(info, 0x33310004);
    putLe32(info, static_cast<std::uint32_t>(entry.size()));
    info.insert(info.end(), entry.begin(), entry.end());

    // DataPart2: 0x7C filler then texture bytes.
    std::vector<std::uint8_t> data2(0x7C, 0);
    data2.insert(data2.end(), dxtBlock.begin(), dxtBlock.end());

    std::vector<std::uint8_t> dataBlock;
    putLe32(dataBlock, 0x33320002);
    putLe32(dataBlock, static_cast<std::uint32_t>(data2.size()));
    dataBlock.insert(dataBlock.end(), data2.begin(), data2.end());

    std::vector<std::uint8_t> file;
    putLe32(file, 0xB3310000);
    putLe32(file, static_cast<std::uint32_t>(info.size()));
    file.insert(file.end(), info.begin(), info.end());
    putLe32(file, 0xB3320000);
    putLe32(file, static_cast<std::uint32_t>(dataBlock.size()));
    file.insert(file.end(), dataBlock.begin(), dataBlock.end());
    return file;
}

} // namespace

TEST_CASE("DXT1 solid-color block decodes exactly") {
    // c0 = c1 = pure red in 565 (0xF800), all indices 0 -> every texel red.
    const std::vector<std::uint8_t> block = {0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
    auto rgba = gfx::decodeDxt(core::ByteSpan(block.data(), block.size()), 4, 4, 1);
    REQUIRE(rgba.size() == 4u * 4u * 4u);
    for (int i = 0; i < 16; ++i) {
        CHECK(rgba[i * 4 + 0] == 255);
        CHECK(rgba[i * 4 + 1] == 0);
        CHECK(rgba[i * 4 + 2] == 0);
        CHECK(rgba[i * 4 + 3] == 255);
    }
}

TEST_CASE("DXT1 transparent mode (c0 <= c1, index 3)") {
    // c0 = 0, c1 = 0xFFFF, all indices 3 -> transparent black.
    const std::vector<std::uint8_t> block = {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    auto rgba = gfx::decodeDxt(core::ByteSpan(block.data(), block.size()), 4, 4, 1);
    REQUIRE(rgba.size() == 64);
    CHECK(rgba[3] == 0);  // alpha
}

TEST_CASE("DXT5 alpha table interpolation") {
    // a0=255 > a1=0: 8-entry ramp; all alpha indices 0 -> 255. Color red.
    const std::vector<std::uint8_t> block = {255, 0,    0,    0,    0, 0, 0, 0,
                                             0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
    auto rgba = gfx::decodeDxt(core::ByteSpan(block.data(), block.size()), 4, 4, 5);
    REQUIRE(rgba.size() == 64);
    CHECK(rgba[0] == 255);
    CHECK(rgba[3] == 255);
}

TEST_CASE("decodeDxt rejects short input") {
    const std::vector<std::uint8_t> tiny = {1, 2, 3};
    CHECK(gfx::decodeDxt(core::ByteSpan(tiny.data(), tiny.size()), 4, 4, 1).empty());
    CHECK(gfx::decodeDxt(core::ByteSpan(tiny.data(), tiny.size()), 4, 4, 7).empty());
}

TEST_CASE("parseTexturePacks reads a synthetic MW TPK") {
    const std::vector<std::uint8_t> dxtBlock = {0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
    std::vector<std::uint8_t> file = buildSyntheticTpk(dxtBlock);

    auto result = formats::parseTexturePacks(core::ByteSpan(file.data(), file.size()));
    REQUIRE(result.ok());
    REQUIRE(result.value().size() == 1);
    const formats::TexturePack& pack = result.value()[0];
    CHECK(pack.name == "TESTPACK");
    CHECK(pack.filename == "tex\\testpack.tpk");
    CHECK(pack.key == 0xCAFEBABE);
    REQUIRE(pack.textures.size() == 1);
    const formats::TextureEntry& tex = pack.textures[0];
    CHECK(tex.name == "TESTTEX");
    CHECK(tex.nameHash == 0x12345678);
    CHECK(tex.width == 4);
    CHECK(tex.height == 4);
    CHECK(tex.textureFormat() == formats::TextureFormat::Dxt1);
    REQUIRE(tex.data.size() == dxtBlock.size());
    CHECK(std::memcmp(tex.data.data(), dxtBlock.data(), dxtBlock.size()) == 0);

    // End-to-end: decode the referenced bytes.
    auto rgba = gfx::decodeDxt(tex.data, tex.width, tex.height, 1);
    REQUIRE(rgba.size() == 64);
    CHECK(rgba[0] == 255);
}

TEST_CASE("parseTexturePacks reads a compressed pack via InfoPart3 offslots") {
    // Blob: 8 bytes of DXT1 + 0x9C trailer (0x7C entry + 0x20 comp info).
    const std::vector<std::uint8_t> dxtBlock = {0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
    std::vector<std::uint8_t> entry;
    for (int i = 0; i < 0xC; ++i) {
        entry.push_back(0);
    }
    putFixedString(entry, "COMPTEX", 0x18);
    putLe32(entry, 0xFEED0001);  // binkey
    putLe32(entry, 0);           // classkey
    putLe32(entry, 0);           // unknown0
    putLe32(entry, 0x4000);      // dataOffset (file-relative, ignored: no palette)
    putLe32(entry, 0);           // paletteOffset
    putLe32(entry, static_cast<std::uint32_t>(dxtBlock.size()));
    putLe32(entry, 0);           // paletteSize
    putLe32(entry, 16);          // area
    putLe16(entry, 4);
    putLe16(entry, 4);
    entry.push_back(2);
    entry.push_back(2);
    entry.push_back(34);  // DXT1
    entry.push_back(0);
    putLe16(entry, 0);
    entry.push_back(1);
    while (entry.size() < 0x9C) {  // pad entry to full 0x9C trailer
        entry.push_back(0);
    }
    std::vector<std::uint8_t> blob = dxtBlock;
    blob.insert(blob.end(), entry.begin(), entry.end());

    // InfoPart1 (0x7C) + InfoPart3 (one 0x18 slot, flags 0 = raw)
    std::vector<std::uint8_t> part1;
    putLe32(part1, 8);
    putFixedString(part1, "COMPPACK", 0x1C);
    putFixedString(part1, "tex\\comppack.tpk", 0x40);
    putLe32(part1, 0);
    while (part1.size() < 0x7C) {
        part1.push_back(0);
    }

    std::vector<std::uint8_t> info;
    putLe32(info, 0x33310001);
    putLe32(info, static_cast<std::uint32_t>(part1.size()));
    info.insert(info.end(), part1.begin(), part1.end());
    const std::size_t infoBlockSize = 8 + info.size() + 8 + 0x18;  // + InfoPart3 chunk
    const std::uint32_t absOffset = static_cast<std::uint32_t>(infoBlockSize + 8);  // in DataBlock
    putLe32(info, 0x33310003);
    putLe32(info, 0x18);
    putLe32(info, 0xFEED0001);  // key
    putLe32(info, absOffset);
    putLe32(info, static_cast<std::uint32_t>(blob.size()));  // encoded
    putLe32(info, static_cast<std::uint32_t>(blob.size()));  // decoded
    info.push_back(0);          // user flags
    info.push_back(0);          // flags = raw
    info.push_back(0);
    info.push_back(0);          // refcount
    putLe32(info, 0);

    std::vector<std::uint8_t> file;
    putLe32(file, 0xB3310000);
    putLe32(file, static_cast<std::uint32_t>(info.size()));
    file.insert(file.end(), info.begin(), info.end());
    putLe32(file, 0xB3320000);
    putLe32(file, static_cast<std::uint32_t>(blob.size()));
    file.insert(file.end(), blob.begin(), blob.end());

    auto result = formats::parseTexturePacks(core::ByteSpan(file.data(), file.size()));
    REQUIRE(result.ok());
    REQUIRE(result.value().size() == 1);
    const formats::TexturePack& pack = result.value()[0];
    CHECK(pack.name == "COMPPACK");
    REQUIRE(pack.textures.size() == 1);
    const formats::TextureEntry& tex = pack.textures[0];
    CHECK(tex.name == "COMPTEX");
    CHECK(tex.nameHash == 0xFEED0001);
    CHECK(tex.textureFormat() == formats::TextureFormat::Dxt1);
    REQUIRE(tex.data.size() == dxtBlock.size());
    CHECK(std::memcmp(tex.data.data(), dxtBlock.data(), dxtBlock.size()) == 0);
}

TEST_CASE("parseTexturePacks returns empty for non-TPK chunks") {
    std::vector<std::uint8_t> file;
    putLe32(file, 0x00034201);  // Tracks chunk
    putLe32(file, 0);
    auto result = formats::parseTexturePacks(core::ByteSpan(file.data(), file.size()));
    REQUIRE(result.ok());
    CHECK(result.value().empty());
}
