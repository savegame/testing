#include "openmw05/app/Viewer.h"

#include "openmw05/core/Log.h"
#include "openmw05/core/Stream.h"
#include "openmw05/formats/TexturePack.h"
#include "openmw05/gfx/DxtDecode.h"
#include "openmw05/gfx/GlContext.h"
#include "openmw05/gfx/ImGuiLayer.h"
#include "openmw05/gfx/RenderTarget.h"
#include "openmw05/gfx/Texture.h"
#include "openmw05/io/Compression.h"

#include <SDL.h>
#include <glad/gles2.h>

#include <algorithm>
#include <sys/stat.h>
#include <vector>

#if defined(OMW05_WITH_IMGUI)
#include <imgui.h>
#endif

namespace omw05 {
namespace app {

namespace {

bool isDirectory(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

int scaledDim(int logical, float scale) {
    return std::max(1, static_cast<int>(static_cast<float>(logical) * scale + 0.5f));
}

const float kRenderScaleSteps[] = {1.0f, 0.75f, 0.5f};

// A decoded + uploaded texture for the M2 in-engine browser.
struct BrowserTexture {
    std::string label;
    gfx::Texture texture;
};

// Loads a chunked file, parses texture packs, decodes what we can and
// uploads to GL. Unsupported formats are listed in the log and skipped.
std::vector<BrowserTexture> loadTextureBrowser(const std::string& path) {
    std::vector<BrowserTexture> out;
    auto file = core::readFile(path);
    if (!file) {
        OMW05_LOG_ERROR("app", "--open: %s", file.error().message.c_str());
        return out;
    }
    static std::vector<std::uint8_t> bytes;  // keep alive: pack spans alias it
    bytes = file.take();
    core::ByteSpan span(bytes.data(), bytes.size());
    auto d = io::decompressAuto(span);
    if (d.ok()) {
        bytes = d.take();
        span = core::ByteSpan(bytes.data(), bytes.size());
    }
    auto packs = formats::parseTexturePacks(span);
    if (!packs) {
        OMW05_LOG_ERROR("app", "--open: %s", packs.error().message.c_str());
        return out;
    }
    using TF = formats::TextureFormat;
    for (const auto& pack : packs.value()) {
        for (const auto& tex : pack.textures) {
            std::vector<std::uint8_t> rgba;
            switch (tex.textureFormat()) {
            case TF::Dxt1:
                rgba = gfx::decodeDxt(tex.data, tex.width, tex.height, 1);
                break;
            case TF::Dxt3:
                rgba = gfx::decodeDxt(tex.data, tex.width, tex.height, 3);
                break;
            case TF::Dxt5:
                rgba = gfx::decodeDxt(tex.data, tex.width, tex.height, 5);
                break;
            case TF::Rgba32:
                if (tex.data.size() >= static_cast<std::size_t>(tex.width) * tex.height * 4) {
                    rgba.assign(tex.data.begin(),
                                tex.data.begin() +
                                    static_cast<std::size_t>(tex.width) * tex.height * 4);
                    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {  // BGRA -> RGBA
                        std::swap(rgba[i], rgba[i + 2]);
                    }
                }
                break;
            default:
                OMW05_LOG_DEBUG("app", "skip '%s': unsupported format %u", tex.name.c_str(),
                                tex.format);
                break;
            }
            if (rgba.empty()) {
                continue;
            }
            auto gl = gfx::Texture::createRgba8(tex.width, tex.height, rgba.data());
            if (gl) {
                out.push_back({pack.name + "/" + tex.name, gl.take()});
            }
        }
    }
    OMW05_LOG_INFO("app", "--open: %zu texture(s) loaded from %s", out.size(), path.c_str());
    return out;
}

} // namespace

core::Result<void> validateGameDir(const std::string& path) {
    if (!isDirectory(path)) {
        return core::Error{core::ErrorCode::IoError, "game dir does not exist: " + path};
    }
    for (const char* sub : {"TRACKS", "GLOBAL", "CARS", "FRONTEND"}) {
        if (!isDirectory(path + "/" + sub)) {
            return core::Error{core::ErrorCode::IoError,
                               "not an NFS:MW (2005, PC) install — missing " + path + "/" + sub +
                                   " (pass the game root via --gamedir or OMW05_GAMEDIR)"};
        }
    }
    return core::Result<void>();
}

core::Result<void> Viewer::run(const ViewerConfig& config) {
    core::Result<gfx::GlContext> ctx =
        gfx::GlContext::create(config.title.c_str(), config.windowWidth, config.windowHeight);
    if (!ctx) {
        return ctx.error();
    }
    gfx::GlContext gl = ctx.take();
    gl.setVsync(config.vsync);

    gfx::Rotation rotation = config.rotation;
    float renderScale = config.renderScale;

    int physW;
    int physH;
    gl.drawableSize(&physW, &physH);
    int logiW;
    int logiH;
    gfx::logicalSize(rotation, physW, physH, &logiW, &logiH);

    // §5a: sceneRT decoupled from window resolution via render scale;
    // uiRT always at native logical resolution so text stays crisp.
    core::Result<gfx::RenderTarget> sceneResult = gfx::RenderTarget::create(
        {scaledDim(logiW, renderScale), scaledDim(logiH, renderScale), false, true});
    if (!sceneResult) {
        return sceneResult.error();
    }
    gfx::RenderTarget sceneRT = sceneResult.take();

    core::Result<gfx::RenderTarget> uiResult =
        gfx::RenderTarget::create({logiW, logiH, false, false});
    if (!uiResult) {
        return uiResult.error();
    }
    gfx::RenderTarget uiRT = uiResult.take();

    core::Result<gfx::Compositor> compResult = gfx::Compositor::create();
    if (!compResult) {
        return compResult.error();
    }
    gfx::Compositor compositor = compResult.take();

    gfx::ImGuiLayer imgui = gfx::ImGuiLayer::create(gl.window(), gl.glHandle());

    std::vector<BrowserTexture> browser;
    if (!config.openFile.empty()) {
        browser = loadTextureBrowser(config.openFile);
    }
    int browserSelected = -1;

    OMW05_LOG_INFO("app", "viewer up: %dx%d physical, %dx%d logical, rotate=%d, scale=%.2f", physW,
                   physH, logiW, logiH, static_cast<int>(rotation), renderScale);

    bool running = true;
    Uint64 prevTicks = SDL_GetPerformanceCounter();
    float frameMs = 0.0f;

    while (running) {
        int winW;
        int winH;
        SDL_GetWindowSize(gl.window(), &winW, &winH);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Mouse input mapping uses window (point) size, matching event coords.
            if (imgui.handleEvent(event, rotation, winW, winH)) {
                continue;
            }
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_F1: {  // cycle render scale (§5a hotkey)
                    const float* it =
                        std::find(std::begin(kRenderScaleSteps), std::end(kRenderScaleSteps),
                                  renderScale);
                    renderScale = (it == std::end(kRenderScaleSteps) ||
                                   it + 1 == std::end(kRenderScaleSteps))
                                      ? kRenderScaleSteps[0]
                                      : *(it + 1);
                    OMW05_LOG_INFO("app", "render scale -> %.2f", renderScale);
                    break;
                }
                case SDLK_F2:  // cycle presentation rotation for quick testing
                    rotation = static_cast<gfx::Rotation>((static_cast<int>(rotation) + 90) % 360);
                    OMW05_LOG_INFO("app", "rotation -> %d", static_cast<int>(rotation));
                    break;
                default:
                    break;
                }
            }
        }

        gl.drawableSize(&physW, &physH);
        gfx::logicalSize(rotation, physW, physH, &logiW, &logiH);
        core::Result<void> r1 =
            sceneRT.resize(scaledDim(logiW, renderScale), scaledDim(logiH, renderScale));
        core::Result<void> r2 = uiRT.resize(logiW, logiH);
        if (!r1 || !r2) {
            return !r1 ? r1.error() : r2.error();
        }

        // --- Scene pass: M0 just clears to a color inside sceneRT (§5a). ---
        sceneRT.bind();
        glClearColor(0.08f, 0.12f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- UI pass into uiRT. ---
        imgui.beginFrame(uiRT);
#if defined(OMW05_WITH_IMGUI)
        if (imgui.active()) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::Begin("OpenMW05 stats");
            ImGui::Text("frame: %.2f ms (%.0f fps)", frameMs,
                        frameMs > 0.0f ? 1000.0f / frameMs : 0.0f);
            ImGui::Text("physical: %dx%d  logical: %dx%d", physW, physH, logiW, logiH);
            ImGui::Text("scene RT: %dx%d", sceneRT.width(), sceneRT.height());
            int rotDeg = static_cast<int>(rotation);
            ImGui::Text("rotation: %d deg (F2)", rotDeg);
            ImGui::SliderFloat("render scale (F1)", &renderScale, 0.25f, 1.0f, "%.2f");
            if (config.gameDir.empty()) {
                ImGui::TextDisabled("no --gamedir set");
            } else {
                ImGui::Text("gamedir: %s", config.gameDir.c_str());
            }
            ImGui::End();

            if (!browser.empty()) {  // M2 texture browser
                ImGui::SetNextWindowPos(ImVec2(10, 160), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(420, 400), ImGuiCond_FirstUseEver);
                ImGui::Begin("Texture browser");
                ImGui::BeginChild("list", ImVec2(180, 0), ImGuiChildFlags_ResizeX);
                for (int i = 0; i < static_cast<int>(browser.size()); ++i) {
                    if (ImGui::Selectable(browser[static_cast<std::size_t>(i)].label.c_str(),
                                          browserSelected == i)) {
                        browserSelected = i;
                    }
                }
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("preview");
                if (browserSelected >= 0 && browserSelected < static_cast<int>(browser.size())) {
                    const gfx::Texture& t =
                        browser[static_cast<std::size_t>(browserSelected)].texture;
                    ImGui::Text("%dx%d", t.width(), t.height());
                    const float avail = ImGui::GetContentRegionAvail().x;
                    const float scaleTo =
                        t.width() > 0 ? std::min(1.0f, avail / static_cast<float>(t.width()))
                                      : 1.0f;
                    ImGui::Image(static_cast<ImTextureID>(t.id()),
                                 ImVec2(static_cast<float>(t.width()) * scaleTo,
                                        static_cast<float>(t.height()) * scaleTo));
                }
                ImGui::EndChild();
                ImGui::End();
            }
        }
#endif
        imgui.endFrame(uiRT);

        // --- Composite: the only pass touching the default framebuffer. ---
        compositor.composite(sceneRT, uiRT, rotation, physW, physH);
        gl.swap();

        const Uint64 now = SDL_GetPerformanceCounter();
        frameMs = static_cast<float>(now - prevTicks) * 1000.0f /
                  static_cast<float>(SDL_GetPerformanceFrequency());
        prevTicks = now;
    }

    return core::Result<void>();
}

} // namespace app
} // namespace omw05
