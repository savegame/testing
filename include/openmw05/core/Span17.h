#pragma once
// Span17: a minimal non-owning byte view. C++17 has no std::span (and C++20
// features are forbidden — CLAUDE.md §2), so the project carries its own.
// Only the byte-oriented API parsers actually need.

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace omw05 {
namespace core {

class ByteSpan {
public:
    ByteSpan() = default;
    ByteSpan(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    const std::uint8_t* data() const { return data_; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    std::uint8_t operator[](std::size_t i) const {
        assert(i < size_);
        return data_[i];
    }

    // Bounds-checked views; out-of-range requests are clamped to empty/short
    // rather than UB, so corrupt sizes in input files can't run off the end.
    ByteSpan subspan(std::size_t offset) const {
        if (offset >= size_) {
            return ByteSpan();
        }
        return ByteSpan(data_ + offset, size_ - offset);
    }
    ByteSpan subspan(std::size_t offset, std::size_t count) const {
        ByteSpan rest = subspan(offset);
        return ByteSpan(rest.data_, count < rest.size_ ? count : rest.size_);
    }

    const std::uint8_t* begin() const { return data_; }
    const std::uint8_t* end() const { return data_ + size_; }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace core
} // namespace omw05
