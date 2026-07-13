#include "openmw05/app/Viewer.h"

#include "openmw05/core/Log.h"
#include "openmw05/core/Stream.h"
#include "openmw05/formats/Solids.h"
#include "openmw05/formats/TexturePack.h"
#include "openmw05/gfx/Camera.h"
#include "openmw05/gfx/DxtDecode.h"
#include "openmw05/gfx/GlContext.h"
#include "openmw05/gfx/ImGuiLayer.h"
#include "openmw05/gfx/Mesh.h"
#include "openmw05/gfx/RenderTarget.h"
#include "openmw05/gfx/Shader.h"
#include "openmw05/gfx/Texture.h"
#include "openmw05/io/Compression.h"

#include <SDL.h>
#include <glad/gles2.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <sys/stat.h>
#include <unordered_map>
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
    std::uint32_t nameHash;
    gfx::Texture texture;
};

// GPU-side model data for the M3 model viewer.
struct ModelPart {
    gfx::MeshPart mesh;
    std::uint32_t diffuseHash;
};

struct Model {
    std::string name;
    float boundsMin[3];
    float boundsMax[3];
    std::vector<gfx::VertexBuffer> vertexBuffers;
    std::vector<ModelPart> parts;
};

struct ViewerAssets {
    std::vector<BrowserTexture> textures;
    std::vector<Model> models;
    std::unordered_map<std::uint32_t, std::uint32_t> textureByHash;  // binHash -> GL id
};

void loadTexturesFrom(core::ByteSpan span, ViewerAssets* assets) {
    auto packs = formats::parseTexturePacks(span);
    if (!packs) {
        OMW05_LOG_ERROR("app", "--open textures: %s", packs.error().message.c_str());
        return;
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
                assets->textureByHash[tex.nameHash] = gl.value().id();
                assets->textures.push_back({pack.name + "/" + tex.name, tex.nameHash, gl.take()});
            }
        }
    }
}

void loadModelsFrom(core::ByteSpan span, ViewerAssets* assets) {
    auto lists = formats::parseSolidLists(span);
    if (!lists) {
        OMW05_LOG_ERROR("app", "--open geometry: %s", lists.error().message.c_str());
        return;
    }
    for (const auto& list : lists.value()) {
        for (const auto& obj : list.objects) {
            Model model;
            model.name = obj.name;
            for (int i = 0; i < 3; ++i) {
                model.boundsMin[i] = obj.boundsMin[i];
                model.boundsMax[i] = obj.boundsMax[i];
            }
            for (const auto& set : obj.vertexSets) {
                if (set.empty()) {
                    // Placeholder so material vertexSetIndex still lines up.
                    static const formats::SolidVertex kZero{};
                    auto vb = gfx::VertexBuffer::create(&kZero, sizeof kZero);
                    if (vb) {
                        model.vertexBuffers.push_back(vb.take());
                    }
                    continue;
                }
                auto vb = gfx::VertexBuffer::create(set.data(),
                                                    set.size() * sizeof(formats::SolidVertex));
                if (vb) {
                    model.vertexBuffers.push_back(vb.take());
                }
            }
            for (const auto& mat : obj.materials) {
                if (mat.indices.empty() || mat.vertexSetIndex >= model.vertexBuffers.size()) {
                    continue;
                }
                auto part = gfx::MeshPart::create(model.vertexBuffers[mat.vertexSetIndex],
                                                  mat.indices.data(), mat.indices.size());
                if (part) {
                    model.parts.push_back({part.take(), mat.diffuseTextureHash});
                }
            }
            if (!model.parts.empty()) {
                assets->models.push_back(std::move(model));
            }
        }
    }
}

ViewerAssets loadAssets(const std::vector<std::string>& paths) {
    ViewerAssets assets;
    for (const std::string& path : paths) {
        auto file = core::readFile(path);
        if (!file) {
            OMW05_LOG_ERROR("app", "--open: %s", file.error().message.c_str());
            continue;
        }
        std::vector<std::uint8_t> bytes = file.take();
        core::ByteSpan span(bytes.data(), bytes.size());
        auto d = io::decompressAuto(span);
        if (d.ok()) {
            bytes = d.take();
            span = core::ByteSpan(bytes.data(), bytes.size());
        }
        loadTexturesFrom(span, &assets);
        loadModelsFrom(span, &assets);
        // bytes freed here: everything referencing them is on the GPU now.
    }
    OMW05_LOG_INFO("app", "--open: %zu texture(s), %zu model(s)", assets.textures.size(),
                   assets.models.size());
    return assets;
}

// GLSL ES 3.00 scene shader: half-lambert, vertex color (BGRA in memory ->
// swizzle), optional diffuse texture.
const char* kSceneVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;  // B,G,R,A byte order
layout(location = 3) in vec2 aUv;
uniform mat4 uViewProj;
out vec3 vNormal;
out vec4 vColor;
out vec2 vUv;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    vNormal = aNormal;
    vColor = vec4(aColor.zyx, aColor.w);
    vUv = aUv;
}
)";

const char* kSceneFs = R"(#version 300 es
precision highp float;
in vec3 vNormal;
in vec4 vColor;
in vec2 vUv;
uniform sampler2D uDiffuse;
uniform int uHasTexture;
out vec4 oColor;
void main() {
    vec3 lightDir = normalize(vec3(0.45, 0.35, 0.82));
    float ndl = dot(normalize(vNormal), lightDir) * 0.5 + 0.5;  // half-lambert
    vec4 base = uHasTexture != 0 ? texture(uDiffuse, vUv) : vec4(0.75, 0.75, 0.78, 1.0);
    if (base.a < 0.35) {
        discard;
    }
    oColor = vec4(base.rgb * vColor.rgb * ndl, 1.0);
}
)";

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

    ViewerAssets assets = loadAssets(config.openFiles);
    int browserSelected = -1;
    int modelSelected = assets.models.empty() ? -1 : 0;

    core::Result<gfx::Shader> sceneShaderResult = gfx::Shader::compile(kSceneVs, kSceneFs);
    if (!sceneShaderResult) {
        return sceneShaderResult.error();
    }
    gfx::Shader sceneShader = sceneShaderResult.take();
    const int locViewProj = sceneShader.uniformLocation("uViewProj");
    const int locDiffuse = sceneShader.uniformLocation("uDiffuse");
    const int locHasTexture = sceneShader.uniformLocation("uHasTexture");

    gfx::OrbitCamera camera;
    auto frameModel = [&camera, &assets](int index) {
        if (index < 0 || index >= static_cast<int>(assets.models.size())) {
            return;
        }
        const Model& m = assets.models[static_cast<std::size_t>(index)];
        float diag = 0;
        for (int i = 0; i < 3; ++i) {
            camera.target[i] = (m.boundsMin[i] + m.boundsMax[i]) * 0.5f;
            const float d = m.boundsMax[i] - m.boundsMin[i];
            diag += d * d;
        }
        diag = std::sqrt(diag);
        camera.distance = diag > 0.01f ? diag * 1.2f : 5.0f;
    };
    frameModel(modelSelected);
    bool orbiting = false;

    OMW05_LOG_INFO("app", "viewer up: %dx%d physical, %dx%d logical, rotate=%d, scale=%.2f", physW,
                   physH, logiW, logiH, static_cast<int>(rotation), renderScale);

    bool running = true;
    Uint64 prevTicks = SDL_GetPerformanceCounter();
    float frameMs = 0.0f;
    int frameCount = 0;

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
            } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                       event.button.button == SDL_BUTTON_LEFT) {
                orbiting = true;
            } else if (event.type == SDL_MOUSEBUTTONUP &&
                       event.button.button == SDL_BUTTON_LEFT) {
                orbiting = false;
            } else if (event.type == SDL_MOUSEMOTION && orbiting) {
                camera.yaw -= static_cast<float>(event.motion.xrel) * 0.01f;
                camera.pitch += static_cast<float>(event.motion.yrel) * 0.01f;
                camera.clampPitch();
            } else if (event.type == SDL_MOUSEWHEEL) {
                camera.distance *= std::pow(0.9f, static_cast<float>(event.wheel.y));
                camera.distance = std::max(camera.distance, 0.05f);
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

        // --- Scene pass into sceneRT (§5a). ---
        sceneRT.bind();
        glClearColor(0.08f, 0.12f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (modelSelected >= 0 && modelSelected < static_cast<int>(assets.models.size())) {
            const Model& model = assets.models[static_cast<std::size_t>(modelSelected)];
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            sceneShader.use();
            float viewProj[16];
            camera.viewProj(static_cast<float>(sceneRT.width()) /
                                static_cast<float>(sceneRT.height()),
                            viewProj);
            glUniformMatrix4fv(locViewProj, 1, GL_FALSE, viewProj);
            glUniform1i(locDiffuse, 0);
            glActiveTexture(GL_TEXTURE0);
            for (const ModelPart& part : model.parts) {
                auto it = assets.textureByHash.find(part.diffuseHash);
                if (it != assets.textureByHash.end()) {
                    glBindTexture(GL_TEXTURE_2D, it->second);
                    glUniform1i(locHasTexture, 1);
                } else {
                    glUniform1i(locHasTexture, 0);
                }
                part.mesh.draw();
            }
            glDisable(GL_DEPTH_TEST);
        }

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

            if (!assets.models.empty()) {  // M3 model viewer controls
                ImGui::SetNextWindowPos(ImVec2(10, 160), ImGuiCond_FirstUseEver);
                ImGui::Begin("Model viewer");
                const std::string& current =
                    assets.models[static_cast<std::size_t>(modelSelected)].name;
                if (ImGui::BeginCombo("object", current.c_str())) {
                    for (int i = 0; i < static_cast<int>(assets.models.size()); ++i) {
                        if (ImGui::Selectable(
                                assets.models[static_cast<std::size_t>(i)].name.c_str(),
                                modelSelected == i)) {
                            modelSelected = i;
                            frameModel(i);
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::Text("parts: %zu",
                            assets.models[static_cast<std::size_t>(modelSelected)].parts.size());
                ImGui::Text("drag: orbit, wheel: zoom");
                ImGui::End();
            }

            if (!assets.textures.empty()) {  // M2 texture browser
                ImGui::SetNextWindowPos(ImVec2(10, 260), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(420, 400), ImGuiCond_FirstUseEver);
                ImGui::Begin("Texture browser");
                ImGui::BeginChild("list", ImVec2(180, 0), ImGuiChildFlags_ResizeX);
                for (int i = 0; i < static_cast<int>(assets.textures.size()); ++i) {
                    if (ImGui::Selectable(
                            assets.textures[static_cast<std::size_t>(i)].label.c_str(),
                            browserSelected == i)) {
                        browserSelected = i;
                    }
                }
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("preview");
                if (browserSelected >= 0 &&
                    browserSelected < static_cast<int>(assets.textures.size())) {
                    const gfx::Texture& t =
                        assets.textures[static_cast<std::size_t>(browserSelected)].texture;
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

        if (!config.screenshotPath.empty() && ++frameCount == 3) {
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(physW) * physH * 4);
            glReadPixels(0, 0, physW, physH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            stbi_flip_vertically_on_write(1);  // GL rows are bottom-up
            if (stbi_write_png(config.screenshotPath.c_str(), physW, physH, 4, pixels.data(),
                               physW * 4)) {
                OMW05_LOG_INFO("app", "screenshot written to %s",
                               config.screenshotPath.c_str());
            } else {
                OMW05_LOG_ERROR("app", "failed to write %s", config.screenshotPath.c_str());
            }
        }

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
