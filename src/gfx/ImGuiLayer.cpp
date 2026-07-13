#if defined(OMW05_WITH_IMGUI)

#include "openmw05/gfx/ImGuiLayer.h"

#include "openmw05/gfx/RenderTarget.h"

#include <SDL.h>
#include <glad/gles2.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

namespace omw05 {
namespace gfx {

ImGuiLayer ImGuiLayer::create(SDL_Window* window, void* glContext) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    // GLES3 renderer path — the backend is compiled with IMGUI_IMPL_OPENGL_ES3.
    ImGui_ImplOpenGL3_Init("#version 300 es");
    ImGuiLayer layer;
    layer.initialized_ = true;
    return layer;
}

ImGuiLayer::ImGuiLayer(ImGuiLayer&& other) noexcept : initialized_(other.initialized_) {
    other.initialized_ = false;
}

ImGuiLayer& ImGuiLayer::operator=(ImGuiLayer&& other) noexcept {
    if (this != &other) {
        this->~ImGuiLayer();
        initialized_ = other.initialized_;
        other.initialized_ = false;
    }
    return *this;
}

ImGuiLayer::~ImGuiLayer() {
    if (initialized_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
    }
}

bool ImGuiLayer::handleEvent(const SDL_Event& event, Rotation rotation, int physicalW,
                             int physicalH) {
    // Rewrite mouse coordinates physical -> logical in ONE place (§5a) so
    // ImGui (which lives in the logical-oriented uiRT) sees correct input.
    SDL_Event mapped = event;
    float lx;
    float ly;
    switch (event.type) {
    case SDL_MOUSEMOTION:
        Compositor::physicalToLogical(rotation, physicalW, physicalH,
                                      static_cast<float>(event.motion.x),
                                      static_cast<float>(event.motion.y), &lx, &ly);
        mapped.motion.x = static_cast<Sint32>(lx);
        mapped.motion.y = static_cast<Sint32>(ly);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        Compositor::physicalToLogical(rotation, physicalW, physicalH,
                                      static_cast<float>(event.button.x),
                                      static_cast<float>(event.button.y), &lx, &ly);
        mapped.button.x = static_cast<Sint32>(lx);
        mapped.button.y = static_cast<Sint32>(ly);
        break;
    default:
        break;
    }
    ImGui_ImplSDL2_ProcessEvent(&mapped);

    const ImGuiIO& io = ImGui::GetIO();
    switch (event.type) {
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
        return io.WantCaptureMouse;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    case SDL_TEXTINPUT:
        return io.WantCaptureKeyboard;
    default:
        return false;
    }
}

void ImGuiLayer::beginFrame(const RenderTarget& uiTarget) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    // The SDL2 backend just set DisplaySize from the window; override with
    // the logical uiRT size so UI layout matches the rotated presentation.
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(uiTarget.width()),
                            static_cast<float>(uiTarget.height()));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame(const RenderTarget& uiTarget) {
    ImGui::Render();
    uiTarget.bind();
    // Transparent clear: the compositor alpha-blends this RT over the scene.
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    RenderTarget::bindDefault();
}

} // namespace gfx
} // namespace omw05

#endif // OMW05_WITH_IMGUI
