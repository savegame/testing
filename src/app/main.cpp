// omw05-viewer entry point: hand-rolled CLI parsing (CLAUDE.md §8), env
// fallbacks OMW05_ROTATE and OMW05_GAMEDIR (§5a, §7).

#include "openmw05/app/Viewer.h"
#include "openmw05/core/Log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void printUsage() {
    std::printf(
        "usage: omw05-viewer [options]\n"
        "  --gamedir <path>       NFS:MW (2005, PC) install dir (env OMW05_GAMEDIR)\n"
        "  --rotate <0|90|180|270> presentation rotation (env OMW05_ROTATE)\n"
        "  --render-scale <f>     3D render scale, 0.25..1.0 (UI stays native)\n"
        "  --no-vsync             disable vsync\n"
        "  --log-level <trace|debug|info|warn|error>\n"
        "  --help                 this text\n"
        "hotkeys: F1 cycle render scale, F2 cycle rotation, Esc quit\n");
}

bool parseLogLevel(const char* s, omw05::core::LogLevel* out) {
    using omw05::core::LogLevel;
    if (!std::strcmp(s, "trace")) {
        *out = LogLevel::Trace;
    } else if (!std::strcmp(s, "debug")) {
        *out = LogLevel::Debug;
    } else if (!std::strcmp(s, "info")) {
        *out = LogLevel::Info;
    } else if (!std::strcmp(s, "warn")) {
        *out = LogLevel::Warn;
    } else if (!std::strcmp(s, "error")) {
        *out = LogLevel::Error;
    } else {
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    omw05::app::ViewerConfig config;

    if (const char* env = std::getenv("OMW05_GAMEDIR")) {
        config.gameDir = env;
    }
    if (const char* env = std::getenv("OMW05_ROTATE")) {
        if (!omw05::gfx::rotationFromDegrees(std::atoi(env), &config.rotation)) {
            std::fprintf(stderr, "OMW05_ROTATE must be 0, 90, 180 or 270 (got '%s')\n", env);
            return 2;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const char* value = (i + 1 < argc) ? argv[i + 1] : nullptr;
        if (!std::strcmp(arg, "--help") || !std::strcmp(arg, "-h")) {
            printUsage();
            return 0;
        } else if (!std::strcmp(arg, "--no-vsync")) {
            config.vsync = false;
        } else if (!std::strcmp(arg, "--gamedir") && value) {
            config.gameDir = value;
            ++i;
        } else if (!std::strcmp(arg, "--rotate") && value) {
            if (!omw05::gfx::rotationFromDegrees(std::atoi(value), &config.rotation)) {
                std::fprintf(stderr, "--rotate must be 0, 90, 180 or 270\n");
                return 2;
            }
            ++i;
        } else if (!std::strcmp(arg, "--render-scale") && value) {
            const float scale = static_cast<float>(std::atof(value));
            if (scale < 0.25f || scale > 1.0f) {
                std::fprintf(stderr, "--render-scale must be in 0.25..1.0\n");
                return 2;
            }
            config.renderScale = scale;
            ++i;
        } else if (!std::strcmp(arg, "--log-level") && value) {
            omw05::core::LogLevel level;
            if (!parseLogLevel(value, &level)) {
                std::fprintf(stderr, "unknown log level '%s'\n", value);
                return 2;
            }
            omw05::core::setLogLevel(level);
            ++i;
        } else {
            std::fprintf(stderr, "unknown option '%s'\n\n", arg);
            printUsage();
            return 2;
        }
    }

    if (!config.gameDir.empty()) {
        omw05::core::Result<void> valid = omw05::app::validateGameDir(config.gameDir);
        if (!valid) {
            OMW05_LOG_ERROR("app", "%s", valid.error().message.c_str());
            return 1;
        }
        OMW05_LOG_INFO("app", "game dir: %s", config.gameDir.c_str());
    } else {
        OMW05_LOG_WARN("app", "no game dir set; running without game data (M0 mode)");
    }

    omw05::core::Result<void> result = omw05::app::Viewer::run(config);
    if (!result) {
        OMW05_LOG_ERROR("app", "fatal: %s", result.error().message.c_str());
        return 1;
    }
    return 0;
}
