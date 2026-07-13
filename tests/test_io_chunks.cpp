// ChunkReader tests on hand-built synthetic buffers (CLAUDE.md §4).
#include <doctest.h>

#include "openmw05/io/ChunkIds.h"
#include "openmw05/io/ChunkReader.h"

#include <cstdint>
#include <vector>

using namespace omw05;

namespace {

void putLe32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x >> 16));
    v.push_back(static_cast<std::uint8_t>(x >> 24));
}

void putChunkHeader(std::vector<std::uint8_t>& v, std::uint32_t id, std::uint32_t size) {
    putLe32(v, id);
    putLe32(v, size);
}

} // namespace

TEST_CASE("walkChunks visits nested containers depth-first") {
    // container(0x80001000) { leaf(0x00001001, 4 bytes) pad(0, 0) } leaf(0x00002000, 0)
    std::vector<std::uint8_t> buf;
    putChunkHeader(buf, 0x80001000u, 12 + 8);
    putChunkHeader(buf, 0x00001001u, 4);
    putLe32(buf, 0xAABBCCDDu);
    putChunkHeader(buf, 0x00000000u, 0);
    putChunkHeader(buf, 0x00002000u, 0);

    struct Seen {
        std::uint32_t id;
        int depth;
        std::size_t size;
    };
    std::vector<Seen> seen;
    auto r = io::walkChunks(core::ByteSpan(buf.data(), buf.size()), [&](const io::Chunk& c, int d) {
        seen.push_back({c.id, d, c.data.size()});
        return true;
    });
    REQUIRE(r.ok());
    REQUIRE(seen.size() == 4);
    CHECK(seen[0].id == 0x80001000u);
    CHECK(seen[0].depth == 0);
    CHECK(seen[1].id == 0x00001001u);
    CHECK(seen[1].depth == 1);
    CHECK(seen[1].size == 4);
    CHECK(seen[2].id == 0x00000000u);
    CHECK(seen[3].id == 0x00002000u);
    CHECK(seen[3].depth == 0);
}

TEST_CASE("walkChunks visitor can skip children") {
    std::vector<std::uint8_t> buf;
    putChunkHeader(buf, 0x80001000u, 8);
    putChunkHeader(buf, 0x00001001u, 0);
    int visits = 0;
    auto r = io::walkChunks(core::ByteSpan(buf.data(), buf.size()), [&](const io::Chunk&, int) {
        ++visits;
        return false;  // never descend
    });
    CHECK(r.ok());
    CHECK(visits == 1);
}

TEST_CASE("walkChunks reports oversized and truncated chunks without UB") {
    std::vector<std::uint8_t> oversized;
    putChunkHeader(oversized, 0x00001001u, 100);  // claims more than remains
    oversized.push_back(0xFF);
    auto r1 = io::walkChunks(core::ByteSpan(oversized.data(), oversized.size()),
                             [&](const io::Chunk&, int) { return true; });
    CHECK_FALSE(r1.ok());
    CHECK(r1.error().code == core::ErrorCode::CorruptData);

    std::vector<std::uint8_t> truncated = {0x01, 0x02, 0x03};  // not even a header
    auto r2 = io::walkChunks(core::ByteSpan(truncated.data(), truncated.size()),
                             [&](const io::Chunk&, int) { return true; });
    CHECK_FALSE(r2.ok());
}

TEST_CASE("chunkIdName resolves known IDs from the Nikki table") {
    CHECK(io::chunkIdName(0x00000000u) == doctest::String("Padding"));
    CHECK(io::chunkIdName(0xB3310000u) == doctest::String("TPK_InfoBlock"));
    CHECK(io::chunkIdName(0x80134000u) == doctest::String("GeometryPack"));
    CHECK(io::chunkIdName(0x00034110u) == doctest::String("TrackStreamingSections"));
    CHECK(io::chunkIdName(0xDEADBEEFu) == nullptr);
}
