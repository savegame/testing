#pragma once
// Generic recursive bChunk container walker (CLAUDE.md §5).
//
// Format (documented in docs/formats/chunks.md, sources: SpeedReflect/Binary,
// Nikki, OpenNFS): a file is a sequence of { uint32 id; uint32 size; }
// little-endian headers, `size` = payload bytes following the header.
// A chunk is a *container* of child chunks when (id & 0x80000000) != 0;
// id 0x00000000 is alignment padding. Every format parser is a set of chunk
// handlers on top of this walker.
//
// Robustness: sizes are clamped to the remaining buffer (corrupt input can
// not read out of bounds); a truncated trailing header stops iteration and
// is reported through Result. Visitors receive non-owning ByteSpans valid
// only while the underlying buffer lives.

#include "openmw05/core/Result.h"
#include "openmw05/core/Span17.h"

#include <cstdint>
#include <functional>

namespace omw05 {
namespace io {

struct Chunk {
    std::uint32_t id = 0;
    core::ByteSpan data;        // payload (children for containers)
    std::size_t fileOffset = 0; // offset of the 8-byte header from buffer start

    bool isContainer() const { return (id & 0x80000000u) != 0; }
    bool isPadding() const { return id == 0; }
};

// Iterates the immediate chunks of `data` (no recursion). Returns an error
// for a truncated header/payload; chunks before the truncation are visited.
core::Result<void> forEachChunk(core::ByteSpan data, std::size_t baseOffset,
                                const std::function<void(const Chunk&)>& visit);

// Depth-first recursive walk. `visit` returning false skips the children of
// a container chunk. maxDepth guards against pathological/corrupt nesting.
core::Result<void> walkChunks(core::ByteSpan data,
                              const std::function<bool(const Chunk&, int depth)>& visit,
                              int maxDepth = 64);

} // namespace io
} // namespace omw05
