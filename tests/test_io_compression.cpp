// Decompressor tests on hand-crafted synthetic streams (CLAUDE.md §4, §5:
// "decompress known hand-crafted samples"). Byte sequences below are encoded
// by hand from the format specs cited in docs/formats/compression.md.
#include <doctest.h>

#include "openmw05/io/Compression.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace omw05;

namespace {

std::string asString(const std::vector<std::uint8_t>& v) {
    return std::string(v.begin(), v.end());
}

std::vector<std::uint8_t> jdlzHeader(std::uint32_t uncompressed, std::uint32_t total) {
    std::vector<std::uint8_t> v = {'J', 'D', 'L', 'Z', 0x02, 0x10, 0x00, 0x00};
    for (int i = 0; i < 4; ++i) {
        v.push_back(static_cast<std::uint8_t>(uncompressed >> (8 * i)));
    }
    for (int i = 0; i < 4; ++i) {
        v.push_back(static_cast<std::uint8_t>(total >> (8 * i)));
    }
    return v;
}

} // namespace

TEST_CASE("RefPack: literal run + stop literals") {
    // Header 0x10FB, 3-byte BE size 5. 0xE0 = 4 literals, 0xFD = stop + 1 literal.
    const std::vector<std::uint8_t> in = {0x10, 0xFB, 0x00, 0x00, 0x05,
                                          0xE0, 'H',  'E',  'L',  'L',
                                          0xFD, 'O'};
    auto r = io::decompressRefPack(core::ByteSpan(in.data(), in.size()));
    REQUIRE(r.ok());
    CHECK(asString(r.value()) == "HELLO");
}

TEST_CASE("RefPack: 2-byte back-reference command") {
    // 'ABAB' literals then copy 4 bytes from distance 2 -> "ABABABAB".
    // 2-byte cmd: b0=0x04 (proceed 0, len (0x04>>2)+3 = 4), b1=1 (dist 2).
    const std::vector<std::uint8_t> in = {0x10, 0xFB, 0x00, 0x00, 0x08, 0xE0, 'A', 'B',
                                          'A',  'B',  0x04, 0x01, 0xFC};
    auto r = io::decompressRefPack(core::ByteSpan(in.data(), in.size()));
    REQUIRE(r.ok());
    CHECK(asString(r.value()) == "ABABABAB");
}

TEST_CASE("RefPack: corrupt streams are rejected, not crashed on") {
    // Back-reference before any output exists.
    const std::vector<std::uint8_t> badRef = {0x10, 0xFB, 0x00, 0x00, 0x04, 0x04, 0x01, 0xFC};
    CHECK_FALSE(io::decompressRefPack(core::ByteSpan(badRef.data(), badRef.size())).ok());

    // Size mismatch: claims 10, produces 5.
    const std::vector<std::uint8_t> shortOut = {0x10, 0xFB, 0x00, 0x00, 0x0A,
                                                0xE0, 'H',  'E',  'L',  'L',
                                                0xFD, 'O'};
    CHECK_FALSE(io::decompressRefPack(core::ByteSpan(shortOut.data(), shortOut.size())).ok());

    const std::vector<std::uint8_t> notRefPack = {0x00, 0x01, 0x02};
    CHECK_FALSE(io::decompressRefPack(core::ByteSpan(notRefPack.data(), notRefPack.size())).ok());
}

TEST_CASE("JDLZ: literal-only stream") {
    // flags bytes are consumed up front (both start at 1); 0x00 flag bytes
    // give 8 literal steps each before the next flag byte is fetched.
    std::vector<std::uint8_t> in = jdlzHeader(10, 29);
    in.push_back(0x00);  // flags1
    in.push_back(0x00);  // flags2
    for (char c : std::string("HELLOWOR")) {
        in.push_back(static_cast<std::uint8_t>(c));
    }
    in.push_back(0x00);  // next flags1 byte after 8 steps
    in.push_back('L');
    in.push_back('D');
    auto r = io::decompressJdlz(core::ByteSpan(in.data(), in.size()));
    REQUIRE(r.ok());
    CHECK(asString(r.value()) == "HELLOWORLD");
}

TEST_CASE("JDLZ: near back-reference") {
    // 'A','B' literals then near-ref (flags2 bit 1): dist=(b0&0x0F)+1=2,
    // len=(b1|((b0&0xF0)<<4))+3=6 -> "ABABABAB".
    std::vector<std::uint8_t> in = jdlzHeader(8, 24);
    in.push_back(0x04);  // flags1: literal, literal, backref
    in.push_back(0x01);  // flags2: first backref uses near form
    in.push_back('A');
    in.push_back('B');
    in.push_back(0x01);  // b0: dist 2, high length bits 0
    in.push_back(0x03);  // b1: length 3+3=6
    auto r = io::decompressJdlz(core::ByteSpan(in.data(), in.size()));
    REQUIRE(r.ok());
    CHECK(asString(r.value()) == "ABABABAB");
}

TEST_CASE("JDLZ: corrupt streams are rejected") {
    std::vector<std::uint8_t> truncated = jdlzHeader(100, 116);
    truncated.push_back(0x00);
    CHECK_FALSE(io::decompressJdlz(core::ByteSpan(truncated.data(), truncated.size())).ok());

    // Back-reference outside the window.
    std::vector<std::uint8_t> badRef = jdlzHeader(5, 22);
    badRef.push_back(0x01);  // flags1: backref first
    badRef.push_back(0x01);  // flags2: near form
    badRef.push_back(0x0F);  // dist 16 > window(0)
    badRef.push_back(0x00);  // len 3
    CHECK_FALSE(io::decompressJdlz(core::ByteSpan(badRef.data(), badRef.size())).ok());
}

TEST_CASE("decompressAuto dispatches by magic") {
    const std::vector<std::uint8_t> refpack = {0x10, 0xFB, 0x00, 0x00, 0x00, 0xFC};
    auto r1 = io::decompressAuto(core::ByteSpan(refpack.data(), refpack.size()));
    REQUIRE(r1.ok());
    CHECK(r1.value().empty());

    const std::vector<std::uint8_t> junk = {1, 2, 3, 4, 5};
    auto r2 = io::decompressAuto(core::ByteSpan(junk.data(), junk.size()));
    CHECK_FALSE(r2.ok());
    CHECK(r2.error().code == core::ErrorCode::UnsupportedData);
}
