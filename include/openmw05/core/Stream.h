#pragma once
// Stream: typed cursor over a ByteSpan (CLAUDE.md §5 — all file access goes
// through explicit typed getters, never struct casts). Reads past the end
// set a sticky fail flag and return zeros; parsers check ok() at checkpoints
// instead of after every getter, which keeps them corrupt-input-safe without
// exceptions (§8).
//
// Also: whole-file loading helper (readFile) returning an owned buffer.

#include "openmw05/core/Endian.h"
#include "openmw05/core/Result.h"
#include "openmw05/core/Span17.h"

#include <cstring>
#include <string>
#include <vector>

namespace omw05 {
namespace core {

class Stream {
public:
    explicit Stream(ByteSpan span) : span_(span) {}

    std::size_t pos() const { return pos_; }
    std::size_t size() const { return span_.size(); }
    std::size_t remaining() const { return pos_ <= span_.size() ? span_.size() - pos_ : 0; }
    bool ok() const { return !failed_; }

    std::uint8_t u8() {
        if (!check(1)) {
            return 0;
        }
        return span_[pos_++];
    }
    std::uint16_t u16() {
        if (!check(2)) {
            return 0;
        }
        std::uint16_t v = readLe16(span_.data() + pos_);
        pos_ += 2;
        return v;
    }
    std::uint32_t u32() {
        if (!check(4)) {
            return 0;
        }
        std::uint32_t v = readLe32(span_.data() + pos_);
        pos_ += 4;
        return v;
    }
    std::uint64_t u64() {
        if (!check(8)) {
            return 0;
        }
        std::uint64_t v = readLe64(span_.data() + pos_);
        pos_ += 8;
        return v;
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
    float f32() {
        std::uint32_t bits = u32();
        float v;
        std::memcpy(&v, &bits, sizeof v);
        return v;
    }

    // Returns a view of `count` bytes (short/empty + fail flag on overrun).
    ByteSpan bytes(std::size_t count) {
        if (!check(count)) {
            return ByteSpan();
        }
        ByteSpan v = span_.subspan(pos_, count);
        pos_ += count;
        return v;
    }

    void skip(std::size_t count) {
        if (check(count)) {
            pos_ += count;
        }
    }
    void seek(std::size_t absolute) {
        if (absolute > span_.size()) {
            failed_ = true;
            pos_ = span_.size();
        } else {
            pos_ = absolute;
        }
    }

private:
    bool check(std::size_t need) {
        if (failed_ || need > remaining()) {
            failed_ = true;
            return false;
        }
        return true;
    }

    ByteSpan span_;
    std::size_t pos_ = 0;
    bool failed_ = false;
};

// Loads a whole file into memory. Game bundles are hundreds of MB at most;
// streaming sections are loaded individually, so whole-file is acceptable.
Result<std::vector<std::uint8_t>> readFile(const std::string& path);

} // namespace core
} // namespace omw05
