#pragma once
// Dear ImGui integration (CLAUDE.md §5b). Renders ONLY into the uiRT so the
// debug UI participates in presentation rotation; consumes rotation-corrected
// (logical) input coordinates via handleEvent().
//
// Compile-time removable: when built without OMW05_WITH_IMGUI this header
// still compiles and every method is an inline no-op, so the app layer needs
// no #ifdefs. ImGui is a dev tool only — the future FNG frontend must not
// depend on it.

#include "openmw05/gfx/Compositor.h"

union SDL_Event;
struct SDL_Window;

namespace omw05 {
namespace gfx {

class RenderTarget;

class ImGuiLayer {
public:
#if defined(OMW05_WITH_IMGUI)
    // Creates the ImGui context and inits the SDL2 + GLES3 backends.
    static ImGuiLayer create(SDL_Window* window, void* glContext);

    ImGuiLayer(ImGuiLayer&& other) noexcept;
    ImGuiLayer& operator=(ImGuiLayer&& other) noexcept;
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ~ImGuiLayer();

    // Feeds an event to ImGui. Mouse coordinates inside are rewritten from
    // physical to logical space using Compositor::physicalToLogical before
    // ImGui sees them. Returns true if ImGui wants to capture the event.
    bool handleEvent(const SDL_Event& event, Rotation rotation, int physicalW, int physicalH);

    // Starts an ImGui frame sized to the (logical) uiRT.
    void beginFrame(const RenderTarget& uiTarget);
    // Renders the ImGui draw data into uiTarget (cleared to transparent).
    void endFrame(const RenderTarget& uiTarget);

    bool active() const { return initialized_; }

private:
    ImGuiLayer() = default;
    bool initialized_ = false;
#else
    static ImGuiLayer create(SDL_Window*, void*) { return ImGuiLayer(); }
    bool handleEvent(const SDL_Event&, Rotation, int, int) { return false; }
    void beginFrame(const RenderTarget&) {}
    void endFrame(const RenderTarget&) {}
    bool active() const { return false; }

private:
    ImGuiLayer() = default;
#endif
};

} // namespace gfx
} // namespace omw05
