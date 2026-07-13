#pragma once
// Resource-name hashes used across all Blackbox-era NFS data (CLAUDE.md §5):
//
// - binHash: 32-bit "bin memory hash" keying textures/solids/FE resources.
//   Algorithm: h = 0xFFFFFFFF; for each byte: h = h*0x21 + byte.
//   Source: Nikki (MaxHwoy) Nikki/Utils/Hashing.cs BinHash, matching all
//   community tooling.
// - vltHash32 / vltHash64: Bob Jenkins lookup2/lookup8-style hashes keying
//   VLT database classes/rows. Ported from VaultLib (NFSTools, MIT):
//   VaultLib.Core/Hashing/VLT32Hasher.cs (init 0xABCDEF00) and
//   VLT64Hasher.cs (init 0xABCDEF0011223344). See THIRD_PARTY.md.
//
// All three hash raw bytes; the game hashes names in their stored case
// (typically upper-case for bin keys — callers decide, we don't case-fold).

#include <cstddef>
#include <cstdint>
#include <string>

namespace omw05 {
namespace core {

std::uint32_t binHash(const void* data, std::size_t size);
inline std::uint32_t binHash(const std::string& s) { return binHash(s.data(), s.size()); }

std::uint32_t vltHash32(const void* data, std::size_t size, std::uint32_t init = 0xABCDEF00u);
inline std::uint32_t vltHash32(const std::string& s) { return vltHash32(s.data(), s.size()); }

std::uint64_t vltHash64(const void* data, std::size_t size,
                        std::uint64_t init = 0xABCDEF0011223344ull);
inline std::uint64_t vltHash64(const std::string& s) { return vltHash64(s.data(), s.size()); }

} // namespace core
} // namespace omw05
