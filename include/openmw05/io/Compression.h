#pragma once
// Decompressors for the two schemes used by NFS:MW PC bundles (CLAUDE.md §5):
//
// - EA RefPack (a.k.a. QFS): LZ77 variant, big-endian 0x10FB signature in a
//   16-bit header. Implemented from the public format documentation on the
//   Niotso wiki ("RefPack") and cross-checked with SpeedReflect/Binary.
// - JDLZ: Blackbox LZ variant with a 16-byte 'JDLZ' header; algorithm
//   documented by the community JDLZ.cs implementations (see
//   docs/formats/compression.md for the layout).
//
// Both are decompress-only for now (the engine never writes game files).
// All input is treated as hostile: any out-of-range reference or truncated
// stream returns CorruptData, never touches memory out of bounds.

#include "openmw05/core/Result.h"
#include "openmw05/core/Span17.h"

#include <cstdint>
#include <vector>

namespace omw05 {
namespace io {

bool isRefPack(core::ByteSpan data);  // checks the 0x10FB signature bits
bool isJdlz(core::ByteSpan data);     // checks the 'JDLZ' magic

core::Result<std::vector<std::uint8_t>> decompressRefPack(core::ByteSpan data);
core::Result<std::vector<std::uint8_t>> decompressJdlz(core::ByteSpan data);

// Dispatches on the magic; UnsupportedData if neither scheme matches.
core::Result<std::vector<std::uint8_t>> decompressAuto(core::ByteSpan data);

} // namespace io
} // namespace omw05
