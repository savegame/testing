#pragma once
// Final composite pass (CLAUDE.md §5a): the ONLY code that draws to the
// default framebuffer. Samples sceneRT (linear-filtered upscale when
// render-scale < 1) then alpha-blends uiRT on top, applying a presentation
// rotation of 0/90/180/270 degrees as a pure UV transform in the shader.
//
// Coordinate model: everything the app renders lives in LOGICAL orientation
// (both RTs are allocated logical-size). For rotation 90/270 the logical
// width/height are the physical drawable's swapped. Input events must be
// mapped physical->logical with physicalToLogical() — one central place.
//
// A post-processing hook (color grading, MW's "yellow filter") can slot in
// later between the scene sample and the UI blend; not implemented now.

#include "openmw05/core/Result.h"
#include "openmw05/gfx/Shader.h"

#include <cstdint>

namespace omw05 {
namespace gfx {

class RenderTarget;

enum class Rotation {
    Deg0 = 0,
    Deg90 = 90,    // content appears rotated 90° clockwise on the display
    Deg180 = 180,
    Deg270 = 270,
};

// Returns false (and leaves out untouched) for anything not 0/90/180/270.
bool rotationFromDegrees(int degrees, Rotation* out);

// Logical size for a physical drawable under a rotation (swaps at 90/270).
void logicalSize(Rotation rotation, int physicalW, int physicalH, int* logicalW, int* logicalH);

class Compositor {
public:
    static core::Result<Compositor> create();

    Compositor(Compositor&& other) noexcept;
    Compositor& operator=(Compositor&& other) noexcept;
    Compositor(const Compositor&) = delete;
    Compositor& operator=(const Compositor&) = delete;
    ~Compositor();

    // Draws scene + UI to the default framebuffer of size physicalW/H.
    // Both targets must be logical-sized; scene may be any resolution
    // (render scale) — it is stretched to fill.
    void composite(const RenderTarget& scene, const RenderTarget& ui, Rotation rotation,
                   int physicalW, int physicalH) const;

    // Maps a physical window coordinate (pixels, origin top-left) to logical
    // coordinates for input handling. THE central inverse-rotation transform.
    static void physicalToLogical(Rotation rotation, int physicalW, int physicalH, float px,
                                  float py, float* lx, float* ly);

private:
    Compositor(Shader shader, std::uint32_t vao);

    Shader shader_;
    std::uint32_t vao_ = 0;  // empty VAO; the quad comes from gl_VertexID
    int locScene_ = -1;
    int locUi_ = -1;
    int locRotation_ = -1;
};

} // namespace gfx
} // namespace omw05
