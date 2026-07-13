// TrackStreamer parsing tests on synthetic buffers (CLAUDE.md §4).
#include <doctest.h>

#include "openmw05/formats/TrackStreamer.h"

#include <cstring>
#include <vector>

using namespace omw05;

namespace {

struct Writer {
    std::vector<std::uint8_t> buf;
    void u8(std::uint8_t v) { buf.push_back(v); }
    void u16(std::uint16_t v) {
        buf.push_back(static_cast<std::uint8_t>(v));
        buf.push_back(static_cast<std::uint8_t>(v >> 8));
    }
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            buf.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void f32(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, 4);
        u32(bits);
    }
    void zeros(std::size_t n) { buf.insert(buf.end(), n, 0); }
    void str(const char* s, std::size_t fixed) {
        std::size_t i = 0;
        for (; s[i]; ++i) {
            buf.push_back(static_cast<std::uint8_t>(s[i]));
        }
        for (; i < fixed; ++i) {
            buf.push_back(0);
        }
    }
    std::size_t beginChunk(std::uint32_t id) {
        u32(id);
        u32(0);
        return buf.size();
    }
    void endChunk(std::size_t payloadStart) {
        const std::uint32_t size = static_cast<std::uint32_t>(buf.size() - payloadStart);
        for (int i = 0; i < 4; ++i) {
            buf[payloadStart - 4 + static_cast<std::size_t>(i)] =
                static_cast<std::uint8_t>(size >> (8 * i));
        }
    }
    void alignTo(std::size_t a) {
        while (buf.size() % a != 0) {
            buf.push_back(0);
        }
    }
};

} // namespace

TEST_CASE("parseStreamingSections reads 0x5C records") {
    Writer w;
    const std::size_t c = w.beginChunk(0x00034110);
    // record: name[8], i16 number, 2+4 skip, fileType, offset, size, csize,
    // permSize, priority, centre xy, radius, checksum, 0x24 runtime bytes.
    w.str("A5", 8);
    w.u16(5);
    w.zeros(2 + 4);
    w.u32(2);           // fileType
    w.u32(0x1000);      // fileOffset
    w.u32(0x2000);      // size
    w.u32(0x2000);      // compressedSize
    w.u32(0x100);       // permSize
    w.u32(0);           // priority
    w.f32(100.0f);      // centre x
    w.f32(-200.0f);     // centre y
    w.f32(350.0f);      // radius
    w.u32(0xC0FFEE);    // checksum
    w.zeros(0x5C - 0x38);
    w.endChunk(c);

    auto r = formats::parseStreamingSections(core::ByteSpan(w.buf.data(), w.buf.size()));
    REQUIRE(r.ok());
    REQUIRE(r.value().size() == 1);
    const formats::StreamingSection& s = r.value()[0];
    CHECK(s.name == "A5");
    CHECK(s.number == 5);
    CHECK(s.fileOffset == 0x1000);
    CHECK(s.size == 0x2000);
    CHECK(s.centre[0] == doctest::Approx(100.0f));
    CHECK(s.centre[1] == doctest::Approx(-200.0f));
    CHECK(s.radius == doctest::Approx(350.0f));
    CHECK(s.checksum == 0xC0FFEE);
}

TEST_CASE("parseScenerySections reads infos and instances") {
    Writer w;
    const std::size_t sect = w.beginChunk(0x80034100);

    {  // header: SectionNumber at +0xC
        const std::size_t c = w.beginChunk(0x00034101);
        w.zeros(0xC);
        w.u32(1500);
        w.zeros(0x2C);
        w.endChunk(c);
    }
    {  // one SceneryInfo (0x48)
        const std::size_t c = w.beginChunk(0x00034102);
        w.str("TREE01", 24);
        w.u32(0x11111111);  // LOD A hash
        w.u32(0x22222222);
        w.u32(0);
        w.u32(0);
        w.zeros(16);       // model pointers
        w.f32(12.5f);      // radius
        w.u32(0);          // checksum
        w.u32(0);          // hierarchy hash
        w.zeros(4);
        w.endChunk(c);
    }
    {  // one SceneryInstance (0x40), payload 16-aligned
        const std::size_t c = w.beginChunk(0x00034103);
        w.alignTo(16);
        w.f32(-1);
        w.f32(-1);
        w.f32(0);  // bbox min
        w.f32(1);
        w.f32(1);
        w.f32(2);  // bbox max
        w.u32(0);  // exclude flags
        w.u16(0);
        w.u16(0);
        w.f32(500.0f);
        w.f32(600.0f);
        w.f32(7.0f);  // position
        // identity rotation in i16/8192 fixed point
        const std::int16_t rot[9] = {8192, 0, 0, 0, 8192, 0, 0, 0, 8192};
        for (std::int16_t v : rot) {
            w.u16(static_cast<std::uint16_t>(v));
        }
        w.u16(0);  // scenery info number
        w.endChunk(c);
    }
    w.endChunk(sect);

    auto r = formats::parseScenerySections(core::ByteSpan(w.buf.data(), w.buf.size()));
    REQUIRE(r.ok());
    REQUIRE(r.value().size() == 1);
    const formats::ScenerySection& s = r.value()[0];
    CHECK(s.sectionNumber == 1500);
    REQUIRE(s.infos.size() == 1);
    CHECK(s.infos[0].debugName == "TREE01");
    CHECK(s.infos[0].modelHash[0] == 0x11111111);
    CHECK(s.infos[0].radius == doctest::Approx(12.5f));
    REQUIRE(s.instances.size() == 1);
    const formats::SceneryInstance& inst = s.instances[0];
    CHECK(inst.position[0] == doctest::Approx(500.0f));
    CHECK(inst.position[2] == doctest::Approx(7.0f));
    CHECK(inst.rotation[0] == doctest::Approx(1.0f));
    CHECK(inst.rotation[4] == doctest::Approx(1.0f));
    CHECK(inst.rotation[1] == doctest::Approx(0.0f));
    CHECK(inst.sceneryInfoNumber == 0);
}
