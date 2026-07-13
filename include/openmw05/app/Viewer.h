#pragma once
// The interactive viewer application (M0: clear-screen + debug UI through the
// offscreen-RT + compositor pipeline; grows into texture/model/world viewer).

#include "openmw05/core/Result.h"
#include "openmw05/gfx/Compositor.h"

#include <string>

namespace omw05 {
namespace app {

struct ViewerConfig {
    std::string title = "OpenMW05";
    int windowWidth = 1280;
    int windowHeight = 720;
    gfx::Rotation rotation = gfx::Rotation::Deg0;  // --rotate / OMW05_ROTATE
    float renderScale = 1.0f;                      // --render-scale, 3D-only (§5a)
    bool vsync = true;
    std::string gameDir;   // may be empty at M0; validated when set (§7)
    std::string openFile;  // --open: chunked file to browse (M2 texture browser)
};

class Viewer {
public:
    // Creates the window/context/RTs and runs the main loop until quit.
    // Returns non-Ok on initialization failure.
    static core::Result<void> run(const ViewerConfig& config);
};

// Validates that `path` looks like an NFS:MW (2005, PC) install: expects
// TRACKS/, GLOBAL/, CARS/, FRONTEND/ subdirectories (CLAUDE.md §7).
core::Result<void> validateGameDir(const std::string& path);

} // namespace app
} // namespace omw05
