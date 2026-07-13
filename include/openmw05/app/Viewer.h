#pragma once
// The interactive viewer application (M0: clear-screen + debug UI through the
// offscreen-RT + compositor pipeline; grows into texture/model/world viewer).

#include "openmw05/core/Result.h"
#include "openmw05/gfx/Compositor.h"

#include <string>
#include <vector>

namespace omw05 {
namespace app {

struct ViewerConfig {
    std::string title = "OpenMW05";
    int windowWidth = 1280;
    int windowHeight = 720;
    gfx::Rotation rotation = gfx::Rotation::Deg0;  // --rotate / OMW05_ROTATE
    float renderScale = 1.0f;                      // --render-scale, 3D-only (§5a)
    bool vsync = true;
    std::string gameDir;  // may be empty at M0; validated when set (§7)
    // --open (repeatable): chunked files to load — texture packs feed the
    // browser (M2), geometry feeds the model viewer (M3). Textures and
    // models from the same invocation are matched by bin hash.
    std::vector<std::string> openFiles;
    // --screenshot: write a PNG of the composited output after a few frames
    // (debug/CI aid; the viewer keeps running).
    std::string screenshotPath;
    float uiScale = 1.0f;  // --ui-scale: ImGui font scale (also a UI slider)
    // --world [id]: M4 streamed world mode (requires --gamedir). Track id
    // defaults to L2RA (Rockport free roam).
    std::string worldTrack;
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
