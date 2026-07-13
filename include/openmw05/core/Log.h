#pragma once
// Minimal logging facility. The logger is the only permitted global mutable
// state in the project (CLAUDE.md §8). printf-style, thread-unsafe by design
// for now (single-threaded engine loop); revisit when TrackStreamer goes async.

#include <cstdarg>

namespace omw05 {
namespace core {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
};

// Global minimum level; messages below it are dropped.
void setLogLevel(LogLevel level);
LogLevel logLevel();

// Core sink: writes "[level] tag: message\n" to stderr.
void logMessage(LogLevel level, const char* tag, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

} // namespace core
} // namespace omw05

// Convenience macros; `tag` identifies the subsystem ("io", "gfx", ...).
#define OMW05_LOG_TRACE(tag, ...) ::omw05::core::logMessage(::omw05::core::LogLevel::Trace, tag, __VA_ARGS__)
#define OMW05_LOG_DEBUG(tag, ...) ::omw05::core::logMessage(::omw05::core::LogLevel::Debug, tag, __VA_ARGS__)
#define OMW05_LOG_INFO(tag, ...) ::omw05::core::logMessage(::omw05::core::LogLevel::Info, tag, __VA_ARGS__)
#define OMW05_LOG_WARN(tag, ...) ::omw05::core::logMessage(::omw05::core::LogLevel::Warn, tag, __VA_ARGS__)
#define OMW05_LOG_ERROR(tag, ...) ::omw05::core::logMessage(::omw05::core::LogLevel::Error, tag, __VA_ARGS__)
