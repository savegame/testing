// Solid geometry parser tests on a synthetic GeometryPack (CLAUDE.md §4).
//
// The builder mirrors how Blackbox writers lay out files: aligned payloads
// (header/groups/indices to 0x10, vertex buffer to 0x80) carry pad bytes at
// the start of the chunk payload so the data lands on an absolute-offset
// boundary — exactly what the parser's alignPad() consumes.
#include <doctest.h>

#include "openmw05/formats/Solids.h"

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
    void str(const char* s, std::size_t fixed = 0) {
        std::size_t i = 0;
        for (; s[i]; ++i) {
            buf.push_back(static_cast<std::uint8_t>(s[i]));
        }
        for (++i; fixed && i <= fixed; ++i) {
            buf.push_back(0);
        }
        if (!fixed) {
            buf.push_back(0);
        }
    }

    // Writes the 8-byte chunk header; returns payload start for endChunk.
    std::size_t beginChunk(std::uint32_t id) {
        u32(id);
        u32(0);  // size patched by endChunk
        return buf.size();
    }
    void endChunk(std::size_t payloadStart) {
        const std::uint32_t size = static_cast<std::uint32_t>(buf.size() - payloadStart);
        for (int i = 0; i < 4; ++i) {
            buf[payloadStart - 4 + static_cast<std::size_t>(i)] =
                static_cast<std::uint8_t>(size >> (8 * i));
        }
    }
    // Pads with zeros until the current absolute offset hits the boundary.
    void alignTo(std::size_t a) {
        while (buf.size() % a != 0) {
            buf.push_back(0);
        }
    }
};

// One triangle, WorldShader (effect 0), stride 36.
std::vector<std::uint8_t> buildSyntheticGeometry() {
    Writer w;
    const std::size_t pack = w.beginChunk(0x80134000);

    {  // header container
        const std::size_t hc = w.beginChunk(0x80134001);
        const std::size_t info = w.beginChunk(0x00134002);
        w.zeros(8);
        w.u32(1);  // marker
        w.u32(1);  // numObjects
        w.str("GEOMETRY.BIN", 0x38);
        w.str("TESTGROUP", 0x20);
        w.endChunk(info);
        w.endChunk(hc);
    }

    {  // solid object container
        const std::size_t obj = w.beginChunk(0x80134010);

        {  // object header, payload aligned to 0x10
            const std::size_t c = w.beginChunk(0x00134011);
            w.alignTo(0x10);
            w.zeros(12);
            w.u8(0x16);  // version
            w.u8(0);
            w.u16(0);
            w.u32(0xAABB0011);  // hash
            w.u16(1);           // numPolys
            w.u16(3);           // numVerts
            w.zeros(4);
            w.zeros(4);
            w.f32(-1);
            w.f32(-1);
            w.f32(0);
            w.zeros(4);
            w.f32(1);
            w.f32(1);
            w.f32(0);
            w.zeros(4);
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    w.f32(i == j ? 1.0f : 0.0f);
                }
            }
            w.zeros(8 + 4 + 4 + 4 + 4 + 4 + 4);
            w.str("TRIANGLE");
            w.endChunk(c);
        }

        {  // texture table
            const std::size_t c = w.beginChunk(0x00134012);
            w.u32(0xDEAD0001);
            w.u32(0);
            w.endChunk(c);
        }

        {  // mesh container
            const std::size_t mesh = w.beginChunk(0x80134100);

            {  // shading groups, aligned 0x10
                const std::size_t c = w.beginChunk(0x00134b02);
                w.alignTo(0x10);
                w.f32(-1);
                w.f32(-1);
                w.f32(0);
                w.f32(1);
                w.f32(1);
                w.f32(0);
                w.u8(0);  // diffuse id
                w.u8(0);  // normal id == diffuse
                w.u8(0);
                w.u8(0);
                w.u8(0);
                w.u8(0);
                w.u16(0);
                w.zeros(0x10);
                w.u32(0);     // effect id = WorldShader
                w.u32(0);     // effect pointer
                w.u32(0x12);  // flags
                w.u32(3);     // numVerts
                w.u32(1);     // numTris
                w.zeros(0x18);
                w.u32(3);  // numIndices
                w.zeros(8);
                w.endChunk(c);
            }

            {  // indices, aligned 0x10
                const std::size_t c = w.beginChunk(0x00134b03);
                w.alignTo(0x10);
                w.u16(0);
                w.u16(1);
                w.u16(2);
                w.endChunk(c);
            }

            {  // vertex buffer, aligned 0x80
                const std::size_t c = w.beginChunk(0x00134b01);
                w.alignTo(0x80);
                const float positions[3][2] = {{0, 0}, {1, 0}, {0, 1}};
                for (const auto& p : positions) {
                    w.f32(p[0]);
                    w.f32(p[1]);
                    w.f32(0);  // position
                    w.f32(0);
                    w.f32(0);
                    w.f32(1);  // normal
                    w.u32(0xFFFFFFFF);
                    w.f32(p[0]);
                    w.f32(p[1]);  // uv
                }
                w.endChunk(c);
            }

            w.endChunk(mesh);
        }

        w.endChunk(obj);
    }

    w.endChunk(pack);
    return w.buf;
}

} // namespace

TEST_CASE("parseSolidLists reads a synthetic triangle object") {
    std::vector<std::uint8_t> file = buildSyntheticGeometry();
    auto result = formats::parseSolidLists(core::ByteSpan(file.data(), file.size()));
    REQUIRE(result.ok());
    REQUIRE(result.value().size() == 1);
    const formats::SolidList& list = result.value()[0];
    CHECK(list.filename == "GEOMETRY.BIN");
    CHECK(list.groupName == "TESTGROUP");
    REQUIRE(list.objects.size() == 1);

    const formats::SolidObject& obj = list.objects[0];
    CHECK(obj.name == "TRIANGLE");
    CHECK(obj.hash == 0xAABB0011);
    CHECK(obj.transform[0] == doctest::Approx(1.0f));
    REQUIRE(obj.textureHashes.size() == 1);
    CHECK(obj.textureHashes[0] == 0xDEAD0001);

    REQUIRE(obj.materials.size() == 1);
    const formats::SolidMaterial& mat = obj.materials[0];
    CHECK(mat.diffuseTextureHash == 0xDEAD0001);
    CHECK(mat.normalTextureHash == 0);
    CHECK(mat.effectId == 0);
    CHECK(mat.numVerts == 3);
    REQUIRE(mat.indices.size() == 3);
    CHECK(mat.indices[2] == 2);

    REQUIRE(obj.vertexSets.size() == 1);
    REQUIRE(obj.vertexSets[0].size() == 3);
    CHECK(obj.vertexSets[0][1].position[0] == doctest::Approx(1.0f));
    CHECK(obj.vertexSets[0][2].uv[1] == doctest::Approx(1.0f));
    CHECK(obj.vertexSets[0][0].normal[2] == doctest::Approx(1.0f));
}

TEST_CASE("parseSolidLists ignores files without geometry") {
    Writer w;
    const std::size_t c = w.beginChunk(0x00034201);
    w.u32(0);
    w.endChunk(c);
    auto result = formats::parseSolidLists(core::ByteSpan(w.buf.data(), w.buf.size()));
    REQUIRE(result.ok());
    CHECK(result.value().empty());
}
