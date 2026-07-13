#include "openmw05/core/Hash.h"

// vltHash32/vltHash64 are ports of VaultLib (NFSTools/VaultLib, MIT,
// (c) 2019 NFS Tools & heyitsleo): VaultLib.Core/Hashing/VLT32Hasher.cs and
// VLT64Hasher.cs. Recorded in THIRD_PARTY.md. The C# helpers' "0x..." string
// passthrough and empty-string special cases are caller policy, not part of
// the hash, and are intentionally omitted here.

namespace omw05 {
namespace core {

std::uint32_t binHash(const void* data, std::size_t size) {
    // Nikki Hashing.BinHash: result = uint.MaxValue; result = result*0x21 + byte.
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    std::uint32_t h = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        h = h * 0x21u + p[i];
    }
    return h;
}

namespace {

// Bob Jenkins lookup2 96-bit mix, as used by VLT32Hasher.cs.
inline void mix32(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c) {
    a -= b; a -= c; a ^= (c >> 13);
    b -= c; b -= a; b ^= (a << 8);
    c -= a; c -= b; c ^= (b >> 13);
    a -= b; a -= c; a ^= (c >> 12);
    b -= c; b -= a; b ^= (a << 16);
    c -= a; c -= b; c ^= (b >> 5);
    a -= b; a -= c; a ^= (c >> 3);
    b -= c; b -= a; b ^= (a << 10);
    c -= a; c -= b; c ^= (b >> 15);
}

inline std::uint32_t le32from(const std::uint8_t* k) {
    return static_cast<std::uint32_t>(k[0]) + (static_cast<std::uint32_t>(k[1]) << 8) +
           (static_cast<std::uint32_t>(k[2]) << 16) + (static_cast<std::uint32_t>(k[3]) << 24);
}

inline std::uint64_t le64from(const std::uint8_t* k) {
    return static_cast<std::uint64_t>(le32from(k)) |
           (static_cast<std::uint64_t>(le32from(k + 4)) << 32);
}

} // namespace

std::uint32_t vltHash32(const void* data, std::size_t size, std::uint32_t init) {
    const std::uint8_t* k = static_cast<const std::uint8_t*>(data);
    std::size_t len = size;
    std::uint32_t a = 0x9e3779b9u;
    std::uint32_t b = a;
    std::uint32_t c = init;

    while (len >= 12) {
        a += le32from(k);
        b += le32from(k + 4);
        c += le32from(k + 8);
        mix32(a, b, c);
        k += 12;
        len -= 12;
    }

    c += static_cast<std::uint32_t>(size);
    switch (len) {  // fallthrough on purpose, exactly as in VLT32Hasher.cs
    case 11: c += static_cast<std::uint32_t>(k[10]) << 24; // fallthrough
    case 10: c += static_cast<std::uint32_t>(k[9]) << 16;  // fallthrough
    case 9: c += static_cast<std::uint32_t>(k[8]) << 8;    // fallthrough
    case 8: b += static_cast<std::uint32_t>(k[7]) << 24;   // fallthrough
    case 7: b += static_cast<std::uint32_t>(k[6]) << 16;   // fallthrough
    case 6: b += static_cast<std::uint32_t>(k[5]) << 8;    // fallthrough
    case 5: b += k[4];                                     // fallthrough
    case 4: a += static_cast<std::uint32_t>(k[3]) << 24;   // fallthrough
    case 3: a += static_cast<std::uint32_t>(k[2]) << 16;   // fallthrough
    case 2: a += static_cast<std::uint32_t>(k[1]) << 8;    // fallthrough
    case 1: a += k[0]; break;
    default: break;
    }
    mix32(a, b, c);
    return c;
}

std::uint64_t vltHash64(const void* data, std::size_t size, std::uint64_t init) {
    // Literal port of VLT64Hasher.cs (a Jenkins lookup8-style hash). Lane
    // mapping from the C# variables: a = `init`, b = `manipulatedInit`,
    // c = `manipulatedPrime`; `lVar5`/`mixVar2` are the temporaries t/m.
    const std::uint8_t* k = static_cast<const std::uint8_t*>(data);
    std::size_t len = size;
    std::uint64_t a = init;
    std::uint64_t b = init;
    std::uint64_t c = 0x9e3779b97f4a7c13ull;

    while (len >= 24) {
        std::uint64_t t = b + le64from(k + 8);
        c += le64from(k + 16);
        b = ((a + le64from(k)) - t - c) ^ (c >> 43);
        a = (t - c - b) ^ (b << 9);
        std::uint64_t m = (c - b - a) ^ (a >> 8);
        b = (b - a - m) ^ (m >> 38);
        a = (a - m - b) ^ (b << 23);
        m = (m - b - a) ^ (a >> 5);
        b = (b - a - m) ^ (m >> 35);
        c = (a - m - b) ^ (b << 49);
        m = (m - b - c) ^ (c >> 11);
        a = (b - c - m) ^ (m >> 12);
        b = (c - m - a) ^ (a << 18);
        c = (m - a - b) ^ (b >> 22);
        k += 24;
        len -= 24;
    }

    c += static_cast<std::uint64_t>(static_cast<std::uint32_t>(size));
    switch (len) {  // fallthrough on purpose, exactly as in VLT64Hasher.cs
    case 23: c += static_cast<std::uint64_t>(k[22]) << 56; // fallthrough
    case 22: c += static_cast<std::uint64_t>(k[21]) << 48; // fallthrough
    case 21: c += static_cast<std::uint64_t>(k[20]) << 40; // fallthrough
    case 20: c += static_cast<std::uint64_t>(k[19]) << 32; // fallthrough
    case 19: c += static_cast<std::uint64_t>(k[18]) << 24; // fallthrough
    case 18: c += static_cast<std::uint64_t>(k[17]) << 16; // fallthrough
    case 17: c += static_cast<std::uint64_t>(k[16]) << 8;  // fallthrough
    case 16: b += static_cast<std::uint64_t>(k[15]) << 56; // fallthrough
    case 15: b += static_cast<std::uint64_t>(k[14]) << 48; // fallthrough
    case 14: b += static_cast<std::uint64_t>(k[13]) << 40; // fallthrough
    case 13: b += static_cast<std::uint64_t>(k[12]) << 32; // fallthrough
    case 12: b += static_cast<std::uint64_t>(k[11]) << 24; // fallthrough
    case 11: b += static_cast<std::uint64_t>(k[10]) << 16; // fallthrough
    case 10: b += static_cast<std::uint64_t>(k[9]) << 8;   // fallthrough
    case 9: b += k[8];                                     // fallthrough
    case 8: a += static_cast<std::uint64_t>(k[7]) << 56;   // fallthrough
    case 7: a += static_cast<std::uint64_t>(k[6]) << 48;   // fallthrough
    case 6: a += static_cast<std::uint64_t>(k[5]) << 40;   // fallthrough
    case 5: a += static_cast<std::uint64_t>(k[4]) << 32;   // fallthrough
    case 4: a += static_cast<std::uint64_t>(k[3]) << 24;   // fallthrough
    case 3: a += static_cast<std::uint64_t>(k[2]) << 16;   // fallthrough
    case 2: a += static_cast<std::uint64_t>(k[1]) << 8;    // fallthrough
    case 1: a += k[0]; break;
    default: break;
    }

    // Final mix, literal port of the C# `default:` block.
    std::uint64_t m1 = (a - b - c) ^ (c >> 43);
    a = (b - c - m1) ^ (m1 << 9);
    std::uint64_t m2 = (c - m1 - a) ^ (a >> 8);
    b = (m1 - a - m2) ^ (m2 >> 38);
    m1 = (a - m2 - b) ^ (b << 23);
    m2 = (m2 - b - m1) ^ (m1 >> 5);
    b = (b - m1 - m2) ^ (m2 >> 35);
    m1 = (m1 - m2 - b) ^ (b << 49);
    m2 = (m2 - b - m1) ^ (m1 >> 11);
    b = (b - m1 - m2) ^ (m2 >> 12);
    m1 = (m1 - m2 - b) ^ (b << 18);
    return (m2 - b - m1) ^ (m1 >> 22);
}

} // namespace core
} // namespace omw05
