#pragma once
// Result<T>: error handling across module boundaries without exceptions
// (CLAUDE.md §8). Parsers return Result and must survive corrupt input.
//
// C++17-only implementation (no std::expected). Holds either a T or an
// Error{code, message}. `Result<void>` is supported for pure status returns.

#include <cassert>
#include <optional>
#include <string>
#include <utility>

namespace omw05 {
namespace core {

enum class ErrorCode {
    Ok = 0,
    IoError,           // file missing/unreadable/short read
    CorruptData,       // structure invariants violated in input bytes
    UnsupportedData,   // valid but not-yet-implemented variant
    InvalidArgument,   // bad caller-supplied parameter
    NotFound,          // lookup by name/hash failed
    GraphicsError,     // GL object creation/completeness failure
};

struct Error {
    ErrorCode code = ErrorCode::Ok;
    std::string message;  // human-readable context, safe to log
};

template <typename T>
class Result {
public:
    // Implicit construction from a value or an Error keeps call sites terse:
    //   return myValue;            return Error{ErrorCode::CorruptData, "..."};
    Result(T value) : ok_(true), value_(std::move(value)) {}
    Result(Error error) : ok_(false), error_(std::move(error)) {
        assert(error_.code != ErrorCode::Ok && "Error result must carry a non-Ok code");
    }

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }

    // Value access: only valid when ok().
    T& value() {
        assert(ok_);
        return *value_;
    }
    const T& value() const {
        assert(ok_);
        return *value_;
    }
    T&& take() {
        assert(ok_);
        return std::move(*value_);
    }

    // Error access: only valid when !ok().
    const Error& error() const {
        assert(!ok_);
        return error_;
    }

private:
    bool ok_;
    std::optional<T> value_;  // optional so T need not be default-constructible
    Error error_{};
};

template <>
class Result<void> {
public:
    Result() : ok_(true) {}
    Result(Error error) : ok_(false), error_(std::move(error)) {
        assert(error_.code != ErrorCode::Ok && "Error result must carry a non-Ok code");
    }

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }

    const Error& error() const {
        assert(!ok_);
        return error_;
    }

private:
    bool ok_;
    Error error_{};
};

} // namespace core
} // namespace omw05
