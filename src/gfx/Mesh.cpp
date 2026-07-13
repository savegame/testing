#include "openmw05/gfx/Mesh.h"

#include <glad/gles2.h>

namespace omw05 {
namespace gfx {

namespace {
constexpr GLsizei kStride = 36;  // formats::SolidVertex
} // namespace

core::Result<VertexBuffer> VertexBuffer::create(const void* data, std::size_t bytes) {
    if (!data || bytes == 0 || bytes % kStride != 0) {
        return core::Error{core::ErrorCode::InvalidArgument, "bad vertex buffer size"};
    }
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return VertexBuffer(vbo);
}

VertexBuffer::VertexBuffer(VertexBuffer&& other) noexcept : vbo_(other.vbo_) { other.vbo_ = 0; }

VertexBuffer& VertexBuffer::operator=(VertexBuffer&& other) noexcept {
    if (this != &other) {
        if (vbo_) {
            glDeleteBuffers(1, &vbo_);
        }
        vbo_ = other.vbo_;
        other.vbo_ = 0;
    }
    return *this;
}

VertexBuffer::~VertexBuffer() {
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
    }
}

core::Result<MeshPart> MeshPart::create(const VertexBuffer& vertices,
                                        const std::uint16_t* indices, std::size_t indexCount) {
    if (!indices || indexCount == 0) {
        return core::Error{core::ErrorCode::InvalidArgument, "empty index buffer"};
    }
    GLuint vao = 0;
    GLuint ebo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vertices.id());
    // Layout mirrors formats::SolidVertex (see Mesh.h).
    glEnableVertexAttribArray(0);  // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);  // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<void*>(12));
    glEnableVertexAttribArray(2);  // color (stored BGRA byte order)
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, kStride,
                          reinterpret_cast<void*>(24));
    glEnableVertexAttribArray(3);  // uv
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<void*>(28));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexCount * 2), indices,
                 GL_STATIC_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return MeshPart(vao, ebo, static_cast<std::uint32_t>(indexCount));
}

MeshPart::MeshPart(MeshPart&& other) noexcept
    : vao_(other.vao_), ebo_(other.ebo_), indexCount_(other.indexCount_) {
    other.vao_ = 0;
    other.ebo_ = 0;
}

MeshPart& MeshPart::operator=(MeshPart&& other) noexcept {
    if (this != &other) {
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
        }
        if (ebo_) {
            glDeleteBuffers(1, &ebo_);
        }
        vao_ = other.vao_;
        ebo_ = other.ebo_;
        indexCount_ = other.indexCount_;
        other.vao_ = 0;
        other.ebo_ = 0;
    }
    return *this;
}

MeshPart::~MeshPart() {
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
    }
    if (ebo_) {
        glDeleteBuffers(1, &ebo_);
    }
}

void MeshPart::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
}

} // namespace gfx
} // namespace omw05
