#include "openmw05/core/Log.h"

#include <cstdio>

namespace omw05 {
namespace core {

namespace {
LogLevel g_level = LogLevel::Info;

const char* levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "trace";
    case LogLevel::Debug:
        return "debug";
    case LogLevel::Info:
        return "info";
    case LogLevel::Warn:
        return "warn";
    case LogLevel::Error:
        return "error";
    }
    return "?";
}
} // namespace

void setLogLevel(LogLevel level) { g_level = level; }

LogLevel logLevel() { return g_level; }

void logMessage(LogLevel level, const char* tag, const char* fmt, ...) {
    if (level < g_level) {
        return;
    }
    std::fprintf(stderr, "[%s] %s: ", levelName(level), tag);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
}

} // namespace core
} // namespace omw05
