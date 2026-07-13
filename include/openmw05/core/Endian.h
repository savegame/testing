#pragma once
// Explicit little-endian byte readers (CLAUDE.md §5): file bytes are NEVER
// reinterpret_cast into structs. All NFS:MW PC formats are little-endian;
// keeping reads explicit lets console (big-endian) variants slot in later.

#include <cstdint>

namespace omw05 {
namespace core {

inline std::uint16_t readLe16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

inline std::uint32_t readLe32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

inline std::uint64_t readLe64(const std::uint8_t* p) {
    return static_cast<std::uint64_t>(readLe32(p)) |
           (static_cast<std::uint64_t>(readLe32(p + 4)) << 32);
}

inline std::uint32_t readBe32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[3]) | (static_cast<std::uint32_t>(p[2]) << 8) |
           (static_cast<std::uint32_t>(p[1]) << 16) | (static_cast<std::uint32_t>(p[0]) << 24);
}

inline std::uint16_t readBe16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[1] | (p[0] << 8));
}

} // namespace core
} // namespace omw05
