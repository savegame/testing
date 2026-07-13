#pragma once
// GL mesh wrappers for the model viewer (GLES3: VAOs, no immediate mode).
//
// The vertex layout matches formats::SolidVertex exactly (36 bytes:
// float3 position, float3 normal, u32 color, float2 uv) so decoded solids
// upload without conversion. formats/ never includes this header.

#include "openmw05/core/Result.h"

#include <cstdint>

namespace omw05 {
namespace gfx {

// One interleaved VBO shared by any number of MeshParts. Move-only RAII.
class VertexBuffer {
public:
    // `bytes` must be a multiple of the 36-byte vertex stride.
    static core::Result<VertexBuffer> create(const void* data, std::size_t bytes);

    VertexBuffer(VertexBuffer&& other) noexcept;
    VertexBuffer& operator=(VertexBuffer&& other) noexcept;
    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;
    ~VertexBuffer();

    std::uint32_t id() const { return vbo_; }

private:
    explicit VertexBuffer(std::uint32_t vbo) : vbo_(vbo) {}
    std::uint32_t vbo_ = 0;
};

// A VAO + u16 element buffer referencing a shared VertexBuffer.
class MeshPart {
public:
    static core::Result<MeshPart> create(const VertexBuffer& vertices,
                                         const std::uint16_t* indices, std::size_t indexCount);

    MeshPart(MeshPart&& other) noexcept;
    MeshPart& operator=(MeshPart&& other) noexcept;
    MeshPart(const MeshPart&) = delete;
    MeshPart& operator=(const MeshPart&) = delete;
    ~MeshPart();

    void draw() const;  // glDrawElements(GL_TRIANGLES, ...)

private:
    MeshPart(std::uint32_t vao, std::uint32_t ebo, std::uint32_t count)
        : vao_(vao), ebo_(ebo), indexCount_(count) {}
    std::uint32_t vao_ = 0;
    std::uint32_t ebo_ = 0;
    std::uint32_t indexCount_ = 0;
};

} // namespace gfx
} // namespace omw05
