#include "openmw05/io/ChunkReader.h"

#include "openmw05/core/Endian.h"

#include <string>

namespace omw05 {
namespace io {

namespace {

core::Result<void> walkImpl(core::ByteSpan data, std::size_t baseOffset,
                            const std::function<bool(const Chunk&, int)>& visit, int depth,
                            int maxDepth) {
    if (depth > maxDepth) {
        return core::Error{core::ErrorCode::CorruptData,
                           "chunk nesting exceeds max depth " + std::to_string(maxDepth)};
    }
    std::size_t pos = 0;
    while (pos + 8 <= data.size()) {
        Chunk chunk;
        chunk.id = core::readLe32(data.data() + pos);
        const std::uint32_t size = core::readLe32(data.data() + pos + 4);
        chunk.fileOffset = baseOffset + pos;
        const std::size_t avail = data.size() - pos - 8;
        if (size > avail) {
            return core::Error{core::ErrorCode::CorruptData,
                               "chunk at offset " + std::to_string(chunk.fileOffset) +
                                   " claims size " + std::to_string(size) + " but only " +
                                   std::to_string(avail) + " bytes remain"};
        }
        chunk.data = data.subspan(pos + 8, size);
        const bool descend = visit(chunk, depth);
        if (descend && chunk.isContainer() && !chunk.data.empty()) {
            core::Result<void> r =
                walkImpl(chunk.data, chunk.fileOffset + 8, visit, depth + 1, maxDepth);
            if (!r) {
                return r;
            }
        }
        pos += 8 + size;
    }
    if (pos != data.size()) {
        return core::Error{core::ErrorCode::CorruptData,
                           "truncated chunk header at offset " + std::to_string(baseOffset + pos)};
    }
    return core::Result<void>();
}

} // namespace

core::Result<void> forEachChunk(core::ByteSpan data, std::size_t baseOffset,
                                const std::function<void(const Chunk&)>& visit) {
    return walkImpl(
        data, baseOffset,
        [&visit](const Chunk& c, int) {
            visit(c);
            return false;  // no recursion
        },
        0, 0);
}

core::Result<void> walkChunks(core::ByteSpan data,
                              const std::function<bool(const Chunk&, int depth)>& visit,
                              int maxDepth) {
    return walkImpl(data, 0, visit, 0, maxDepth);
}

} // namespace io
} // namespace omw05
