#include "openmw05/io/Compression.h"

#include "openmw05/core/Endian.h"

#include <cstring>
#include <string>

namespace omw05 {
namespace io {

namespace {

core::Error corrupt(const char* what) {
    return core::Error{core::ErrorCode::CorruptData, std::string("decompress: ") + what};
}

// Copies a back-reference byte-by-byte (overlapping copies are the point).
bool copyBackRef(std::vector<std::uint8_t>& out, std::size_t distance, std::size_t length) {
    if (distance == 0 || distance > out.size()) {
        return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
        out.push_back(out[out.size() - distance]);
    }
    return true;
}

} // namespace

bool isRefPack(core::ByteSpan data) {
    // 16-bit big-endian header; signature bits per Niotso wiki "RefPack":
    // (header & 0x3EFF) == 0x10FB.
    if (data.size() < 2) {
        return false;
    }
    return (core::readBe16(data.data()) & 0x3EFFu) == 0x10FBu;
}

bool isJdlz(core::ByteSpan data) {
    return data.size() >= 16 && data[0] == 'J' && data[1] == 'D' && data[2] == 'L' &&
           data[3] == 'Z';
}

core::Result<std::vector<std::uint8_t>> decompressRefPack(core::ByteSpan in) {
    if (!isRefPack(in)) {
        return corrupt("not a RefPack stream");
    }
    const std::uint16_t flags = core::readBe16(in.data());
    const bool largeSizes = (flags & 0x8000u) != 0;         // 4-byte size fields
    const bool hasCompressedSize = (flags & 0x0100u) != 0;  // extra size field
    const std::size_t sizeBytes = largeSizes ? 4 : 3;

    std::size_t pos = 2;
    auto readSize = [&](std::size_t& outValue) -> bool {
        if (pos + sizeBytes > in.size()) {
            return false;
        }
        std::size_t v = 0;
        for (std::size_t i = 0; i < sizeBytes; ++i) {  // big-endian
            v = (v << 8) | in[pos + i];
        }
        pos += sizeBytes;
        outValue = v;
        return true;
    };

    std::size_t skipped;
    if (hasCompressedSize && !readSize(skipped)) {
        return corrupt("RefPack truncated in compressed-size field");
    }
    std::size_t uncompressedSize;
    if (!readSize(uncompressedSize)) {
        return corrupt("RefPack truncated in uncompressed-size field");
    }
    // 512 MiB sanity cap: nothing in the game is close; guards corrupt sizes.
    if (uncompressedSize > (512u << 20)) {
        return corrupt("RefPack size implausibly large");
    }

    std::vector<std::uint8_t> out;
    out.reserve(uncompressedSize);

    // Command stream per Niotso wiki. proceed = literal bytes copied before
    // the command's back-reference (if any).
    while (pos < in.size()) {
        const std::uint8_t b0 = in[pos];
        std::size_t proceed;
        std::size_t refLen = 0;
        std::size_t refDist = 0;
        if (b0 < 0x80) {  // 2-byte command
            if (pos + 2 > in.size()) {
                return corrupt("RefPack truncated 2-byte command");
            }
            const std::uint8_t b1 = in[pos + 1];
            proceed = b0 & 0x03u;
            refLen = ((b0 & 0x1Cu) >> 2) + 3;
            refDist = (static_cast<std::size_t>(b0 & 0x60u) << 3) + b1 + 1;
            pos += 2;
        } else if (b0 < 0xC0) {  // 3-byte command
            if (pos + 3 > in.size()) {
                return corrupt("RefPack truncated 3-byte command");
            }
            const std::uint8_t b1 = in[pos + 1];
            const std::uint8_t b2 = in[pos + 2];
            proceed = (b1 >> 6) & 0x03u;
            refLen = (b0 & 0x3Fu) + 4;
            refDist = (static_cast<std::size_t>(b1 & 0x3Fu) << 8) + b2 + 1;
            pos += 3;
        } else if (b0 < 0xE0) {  // 4-byte command
            if (pos + 4 > in.size()) {
                return corrupt("RefPack truncated 4-byte command");
            }
            const std::uint8_t b1 = in[pos + 1];
            const std::uint8_t b2 = in[pos + 2];
            const std::uint8_t b3 = in[pos + 3];
            proceed = b0 & 0x03u;
            refLen = (static_cast<std::size_t>(b0 & 0x0Cu) << 6) + b3 + 5;
            refDist = (static_cast<std::size_t>(b0 & 0x10u) << 12) +
                      (static_cast<std::size_t>(b1) << 8) + b2 + 1;
            pos += 4;
        } else if (b0 < 0xFC) {  // literal run
            proceed = (static_cast<std::size_t>(b0 & 0x1Fu) + 1) << 2;
            pos += 1;
        } else {  // stop command with 0..3 trailing literals
            proceed = b0 & 0x03u;
            pos += 1;
            if (pos + proceed > in.size()) {
                return corrupt("RefPack truncated stop literals");
            }
            core::ByteSpan lit = in.subspan(pos, proceed);
            out.insert(out.end(), lit.begin(), lit.end());
            break;
        }

        if (pos + proceed > in.size()) {
            return corrupt("RefPack truncated literals");
        }
        core::ByteSpan lit = in.subspan(pos, proceed);
        out.insert(out.end(), lit.begin(), lit.end());
        pos += proceed;

        if (refLen > 0 && !copyBackRef(out, refDist, refLen)) {
            return corrupt("RefPack back-reference outside window");
        }
    }

    if (out.size() != uncompressedSize) {
        return corrupt("RefPack output size mismatch");
    }
    return out;
}

core::Result<std::vector<std::uint8_t>> decompressJdlz(core::ByteSpan in) {
    // 16-byte header: 'JDLZ', u8 version(0x02), u8 0x10, u16 flags(0),
    // u32 LE uncompressed size, u32 LE total (file) size. Layout per the
    // community JDLZ.cs (docs/formats/compression.md).
    if (!isJdlz(in)) {
        return corrupt("not a JDLZ stream");
    }
    const std::size_t uncompressedSize = core::readLe32(in.data() + 8);
    if (uncompressedSize > (512u << 20)) {
        return corrupt("JDLZ size implausibly large");
    }

    std::vector<std::uint8_t> out;
    out.reserve(uncompressedSize);

    std::size_t pos = 16;
    std::uint32_t flags1 = 1;
    std::uint32_t flags2 = 1;

    while (pos < in.size() && out.size() < uncompressedSize) {
        if (flags1 == 1) {
            if (pos >= in.size()) {
                break;
            }
            flags1 = in[pos++] | 0x100u;
        }
        if (flags2 == 1) {
            if (pos >= in.size()) {
                break;
            }
            flags2 = in[pos++] | 0x100u;
        }

        if (flags1 & 1) {  // back-reference, two encoding forms
            if (pos + 2 > in.size()) {
                return corrupt("JDLZ truncated back-reference");
            }
            std::size_t length;
            std::size_t distance;
            if (flags2 & 1) {  // near: distance 1..16, length 3..4098
                length = (in[pos + 1] | (static_cast<std::size_t>(in[pos] & 0xF0u) << 4)) + 3;
                distance = (in[pos] & 0x0Fu) + 1;
            } else {  // far: distance 17..2064, length 3..34
                distance = (in[pos + 1] | (static_cast<std::size_t>(in[pos] & 0xE0u) << 3)) + 17;
                length = (in[pos] & 0x1Fu) + 3;
            }
            pos += 2;
            if (out.size() + length > uncompressedSize) {
                return corrupt("JDLZ back-reference overflows output");
            }
            if (!copyBackRef(out, distance, length)) {
                return corrupt("JDLZ back-reference outside window");
            }
            flags2 >>= 1;
        } else {  // literal
            if (out.size() < uncompressedSize) {
                out.push_back(in[pos++]);
            }
        }
        flags1 >>= 1;
    }

    if (out.size() != uncompressedSize) {
        return corrupt("JDLZ output size mismatch");
    }
    return out;
}

core::Result<std::vector<std::uint8_t>> decompressAuto(core::ByteSpan data) {
    if (isJdlz(data)) {
        return decompressJdlz(data);
    }
    if (isRefPack(data)) {
        return decompressRefPack(data);
    }
    return core::Error{core::ErrorCode::UnsupportedData, "unknown compression magic"};
}

} // namespace io
} // namespace omw05
