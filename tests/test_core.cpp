// Core primitive tests — synthetic buffers only (CLAUDE.md §4).
#include <doctest.h>

#include "openmw05/core/Hash.h"
#include "openmw05/core/Span17.h"
#include "openmw05/core/Stream.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace omw05::core;

TEST_CASE("ByteSpan bounds are clamped, never UB") {
    const std::uint8_t buf[4] = {1, 2, 3, 4};
    ByteSpan s(buf, 4);
    CHECK(s.size() == 4);
    CHECK(s.subspan(2).size() == 2);
    CHECK(s.subspan(4).empty());
    CHECK(s.subspan(100).empty());
    CHECK(s.subspan(1, 100).size() == 3);
    CHECK(s.subspan(3, 0).empty());
}

TEST_CASE("Stream reads little-endian and fails sticky on overrun") {
    const std::uint8_t buf[] = {0x78, 0x56, 0x34, 0x12, 0xEF, 0xBE, 0xAD, 0xDE, 0xAA};
    Stream st(ByteSpan(buf, sizeof buf));
    CHECK(st.u32() == 0x12345678u);
    CHECK(st.u32() == 0xDEADBEEFu);
    CHECK(st.u8() == 0xAA);
    CHECK(st.ok());
    CHECK(st.remaining() == 0);
    CHECK(st.u16() == 0);  // overrun
    CHECK_FALSE(st.ok());
    st.seek(0);
    CHECK_FALSE(st.ok());  // fail flag is sticky
}

TEST_CASE("Stream u64/f32/bytes") {
    const std::uint8_t buf[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
                                0x00, 0x00, 0x80, 0x3F, 'a',  'b',  'c'};
    Stream st(ByteSpan(buf, sizeof buf));
    CHECK(st.u64() == 0x8000000000000001ull);
    CHECK(st.f32() == doctest::Approx(1.0f));
    ByteSpan tail = st.bytes(3);
    REQUIRE(tail.size() == 3);
    CHECK(tail[0] == 'a');
    CHECK(st.ok());
}

TEST_CASE("binHash matches the community algorithm") {
    // h("") = init; h("A") = 0xFFFFFFFF*0x21 + 'A' mod 2^32 = 0x20.
    CHECK(binHash(std::string("")) == 0xFFFFFFFFu);
    CHECK(binHash(std::string("A")) == 0x20u);
    // Incremental property: h(xy) = h(x)*33 + y.
    const std::uint32_t hx = binHash(std::string("GLOBAL"));
    CHECK(binHash(std::string("GLOBALB")) == hx * 0x21u + static_cast<std::uint8_t>('B'));
}

TEST_CASE("vltHash32 reference vectors (computed from VaultLib VLT32Hasher.cs)") {
    // Vectors cross-checked against an independent transcription of the C#
    // source (see tests/tools/vlt_hash_reference.py).
    CHECK(vltHash32(std::string("a")) == 0x628300E4u);
    CHECK(vltHash32(std::string("pvehicle")) == 0x4A97EC8Fu);
    CHECK(vltHash32(std::string("aaaabbbbccccdd")) == 0xDCBDF8E1u);
}

TEST_CASE("vltHash64 reference vectors (computed from VaultLib VLT64Hasher.cs)") {
    CHECK(vltHash64(std::string("a")) == 0x8A8D3CDDC0C29A00ull);
    CHECK(vltHash64(std::string("pvehicle")) == 0x44B78C9B8DDA5D34ull);
    CHECK(vltHash64(std::string("abcdefghijklmnopqrstuvwxyz0123456789")) ==
          0xBCC996AE08F03857ull);
}
