#include "openmw05/app/Viewer.h"

#include "openmw05/core/Log.h"
#include "openmw05/core/Stream.h"
#include "openmw05/formats/Solids.h"
#include "openmw05/formats/TextureDecode.h"
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
#include <dirent.h>
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
    std::vector<formats::PositionMarker> markers;
    bool isWheel = false;  // name contains "WHEEL": replicated over markers
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
    for (const auto& pack : packs.value()) {
        for (const auto& tex : pack.textures) {
            std::vector<std::uint8_t> rgba = formats::decodeTextureRgba(tex);
            if (rgba.empty()) {
                OMW05_LOG_WARN("app", "texture '%s' (0x%08X): undecoded format %s (%u)",
                               tex.name.c_str(), tex.nameHash,
                               formats::textureFormatName(tex.textureFormat()), tex.format);
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
            model.markers = obj.markers;
            model.isWheel = obj.name.find("WHEEL") != std::string::npos ||
                            obj.name.find("TIRE") != std::string::npos;
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

// Lists CARS/<name> directories that contain a GEOMETRY.BIN.
std::vector<std::string> listCarDirs(const std::string& gamedir) {
    std::vector<std::string> cars;
    const std::string carsRoot = gamedir + "/CARS";
    DIR* dir = opendir(carsRoot.c_str());
    if (!dir) {
        return cars;
    }
    while (dirent* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        struct stat st {};
        if (stat((carsRoot + "/" + name + "/GEOMETRY.BIN").c_str(), &st) == 0) {
            cars.push_back(name);
        }
    }
    closedir(dir);
    std::sort(cars.begin(), cars.end());
    return cars;
}

// Car parts carry a trailing LOD letter: "..._KIT00_BASE_A" (A = highest).
// Returns the letter or 0 when the name has no LOD suffix.
char lodSuffix(const std::string& name) {
    if (name.size() >= 2 && name[name.size() - 2] == '_') {
        const char c = name.back();
        if (c >= 'A' && c <= 'E') {
            return c;
        }
    }
    return 0;
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
uniform mat4 uModel;
out vec3 vNormal;
out vec4 vColor;
out vec2 vUv;
void main() {
    gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * aNormal;
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

    // Car browser state (auto-scanned from --gamedir).
    std::vector<std::string> cars =
        config.gameDir.empty() ? std::vector<std::string>() : listCarDirs(config.gameDir);
    int carSelected = -1;
    float uiScale = config.uiScale;
    bool needFrame = false;   // re-frame the camera on next draw-list build
    bool placeWheels = false;  // markers are lights/exhausts; real wheel
                              // positions come from VLT pvehicle data (M5)
    std::vector<char> visible;  // per-model draw checkbox state

    // Preset: show only parts of one LOD (plus suffixless objects).
    auto applyLodPreset = [&](char lod) {
        for (std::size_t i = 0; i < assets.models.size(); ++i) {
            const char l = lodSuffix(assets.models[i].name);
            visible[i] = (l == 0 || l == lod) ? 1 : 0;
        }
        needFrame = true;
    };
    // Preset: a plausible stock car — KIT00 parts at LOD A plus wheels,
    // skipping STYLE*/DAMAGE variants and decal/spoiler extras. The real
    // stock configuration lives in the VLT database (M5); heuristic until
    // then.
    auto applyStockPreset = [&]() {
        bool hoodPicked = false;
        for (std::size_t i = 0; i < assets.models.size(); ++i) {
            const std::string& n = assets.models[i].name;
            const char l = lodSuffix(n);
            bool on = false;
            if ((l == 'A' || l == 0) && n.find("DAMAGE") == std::string::npos &&
                n.find("STYLE") == std::string::npos) {
                if (assets.models[i].isWheel) {
                    on = true;
                } else if (n.find("_BASE_") != std::string::npos &&
                           n.find("_KIT") == std::string::npos) {
                    on = true;  // glass/base shell
                } else if (n.find("_KIT00_") != std::string::npos) {
                    const bool isHood = n.find("HOOD") != std::string::npos;
                    const bool isExtra = n.find("DECAL") != std::string::npos ||
                                         n.find("SPOILER") != std::string::npos ||
                                         n.find("ROOF") != std::string::npos;
                    if (isHood) {
                        on = !hoodPicked;  // exactly one hood
                        hoodPicked = hoodPicked || on;
                    } else {
                        on = !isExtra;
                    }
                }
            }
            visible[i] = on ? 1 : 0;
        }
        needFrame = true;
    };
    auto resetVisibility = [&]() {
        visible.assign(assets.models.size(), 0);
        if (!assets.models.empty()) {
            applyStockPreset();
        }
    };
    resetVisibility();

    auto loadCar = [&](int index) {
        if (index < 0 || index >= static_cast<int>(cars.size())) {
            return;
        }
        const std::string base = config.gameDir + "/CARS/" + cars[static_cast<std::size_t>(index)];
        // GLOBALB.BUN carries shared car textures (windows, plates, driver).
        assets = loadAssets({base + "/GEOMETRY.BIN", base + "/TEXTURES.BIN",
                             config.gameDir + "/GLOBAL/GLOBALB.BUN"});
        modelSelected = assets.models.empty() ? -1 : 0;
        browserSelected = -1;
        carSelected = index;
        resetVisibility();
    };

    core::Result<gfx::Shader> sceneShaderResult = gfx::Shader::compile(kSceneVs, kSceneFs);
    if (!sceneShaderResult) {
        return sceneShaderResult.error();
    }
    gfx::Shader sceneShader = sceneShaderResult.take();
    const int locViewProj = sceneShader.uniformLocation("uViewProj");
    const int locModel = sceneShader.uniformLocation("uModel");
    const int locDiffuse = sceneShader.uniformLocation("uDiffuse");
    const int locHasTexture = sceneShader.uniformLocation("uHasTexture");
    static const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    gfx::OrbitCamera camera;
    // Frames the camera on the union bounds of the given model indices.
    auto frameModels = [&camera, &assets](const std::vector<int>& indices) {
        if (indices.empty()) {
            return;
        }
        float mn[3] = {1e30f, 1e30f, 1e30f};
        float mx[3] = {-1e30f, -1e30f, -1e30f};
        for (int index : indices) {
            const Model& m = assets.models[static_cast<std::size_t>(index)];
            for (int i = 0; i < 3; ++i) {
                mn[i] = std::min(mn[i], m.boundsMin[i]);
                mx[i] = std::max(mx[i], m.boundsMax[i]);
            }
        }
        float diag = 0;
        for (int i = 0; i < 3; ++i) {
            camera.target[i] = (mn[i] + mx[i]) * 0.5f;
            const float d = mx[i] - mn[i];
            diag += d * d;
        }
        diag = std::sqrt(diag);
        camera.distance = diag > 0.01f ? diag * 1.2f : 5.0f;
    };
    // Models drawn this frame = the checked ones.
    auto buildDrawList = [&]() {
        std::vector<int> list;
        for (int i = 0; i < static_cast<int>(assets.models.size()); ++i) {
            if (visible[static_cast<std::size_t>(i)]) {
                list.push_back(i);
            }
        }
        return list;
    };
    needFrame = !assets.models.empty();
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

        const std::vector<int> drawList = buildDrawList();
        if (needFrame) {
            frameModels(drawList);
            needFrame = false;
        }
        if (!drawList.empty()) {
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

            // Wheel placement: a WHEEL model is drawn once per position
            // marker (its own, or borrowed from a visible body that has
            // markers). Marker matrices carry translation in row 3; the
            // row-vector convention matches GL column-major memory layout.
            std::vector<formats::PositionMarker> wheelMarkers;
            for (int modelIdx : drawList) {
                const Model& m = assets.models[static_cast<std::size_t>(modelIdx)];
                if (!m.isWheel && m.markers.size() >= 4) {
                    wheelMarkers = m.markers;  // wheel slots have the widest |y|
                    std::sort(wheelMarkers.begin(), wheelMarkers.end(),
                              [](const formats::PositionMarker& a,
                                 const formats::PositionMarker& b) {
                                  return std::fabs(a.matrix[13]) > std::fabs(b.matrix[13]);
                              });
                    wheelMarkers.resize(4);
                    break;
                }
            }

            for (int modelIdx : drawList) {
                const Model& model = assets.models[static_cast<std::size_t>(modelIdx)];
                std::vector<const float*> instances;
                if (placeWheels && model.isWheel && !wheelMarkers.empty()) {
                    for (const auto& marker : wheelMarkers) {
                        instances.push_back(marker.matrix);
                    }
                }
                if (instances.empty()) {
                    instances.push_back(kIdentity);
                }

                for (const float* transform : instances) {
                    glUniformMatrix4fv(locModel, 1, GL_FALSE, transform);
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
                }
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
            ImGui::SliderFloat("UI scale", &uiScale, 0.5f, 3.0f, "%.1f");
            if (config.gameDir.empty()) {
                ImGui::TextDisabled("no --gamedir set");
            } else {
                ImGui::Text("gamedir: %s", config.gameDir.c_str());
            }
            ImGui::End();

            ImGui::GetIO().FontGlobalScale = uiScale;

            if (!cars.empty()) {  // car browser (auto-scanned from --gamedir)
                ImGui::SetNextWindowPos(ImVec2(440, 10), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(260, 400), ImGuiCond_FirstUseEver);
                ImGui::Begin("Cars");
                for (int i = 0; i < static_cast<int>(cars.size()); ++i) {
                    ImGui::PushID(i);
                    if (ImGui::Selectable(cars[static_cast<std::size_t>(i)].c_str(),
                                          carSelected == i)) {
                        loadCar(i);
                    }
                    ImGui::PopID();
                }
                ImGui::End();
            }

            if (!assets.models.empty()) {  // M3 model viewer controls
                ImGui::SetNextWindowPos(ImVec2(10, 160), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(360, 460), ImGuiCond_FirstUseEver);
                ImGui::Begin("Model viewer");
                if (ImGui::Button("Stock")) {
                    applyStockPreset();
                }
                ImGui::SameLine();
                if (ImGui::Button("LOD A")) {
                    applyLodPreset('A');
                }
                ImGui::SameLine();
                if (ImGui::Button("LOD B")) {
                    applyLodPreset('B');
                }
                ImGui::SameLine();
                if (ImGui::Button("All")) {
                    std::fill(visible.begin(), visible.end(), char(1));
                    needFrame = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("None")) {
                    std::fill(visible.begin(), visible.end(), char(0));
                }
                ImGui::SameLine();
                if (ImGui::Button("Frame")) {
                    needFrame = true;
                }
                ImGui::Checkbox("place wheels at markers (wrong until VLT/M5)", &placeWheels);
                ImGui::Text("objects: %zu, textures: %zu", assets.models.size(),
                            assets.textures.size());
                ImGui::Separator();
                ImGui::BeginChild("modelchecks");
                for (int i = 0; i < static_cast<int>(assets.models.size()); ++i) {
                    const Model& m = assets.models[static_cast<std::size_t>(i)];
                    bool on = visible[static_cast<std::size_t>(i)] != 0;
                    char label[160];
                    std::snprintf(label, sizeof label, "%s (%zu parts, %zu markers)",
                                  m.name.c_str(), m.parts.size(), m.markers.size());
                    ImGui::PushID(i);  // object names can repeat across packs
                    if (ImGui::Checkbox(label, &on)) {
                        visible[static_cast<std::size_t>(i)] = on ? 1 : 0;
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::End();
            }

            if (!assets.textures.empty()) {  // M2 texture browser
                ImGui::SetNextWindowPos(ImVec2(10, 260), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(420, 400), ImGuiCond_FirstUseEver);
                ImGui::Begin("Texture browser");
                ImGui::BeginChild("list", ImVec2(180, 0), ImGuiChildFlags_ResizeX);
                for (int i = 0; i < static_cast<int>(assets.textures.size()); ++i) {
                    ImGui::PushID(i);  // labels repeat when packs share names
                    if (ImGui::Selectable(
                            assets.textures[static_cast<std::size_t>(i)].label.c_str(),
                            browserSelected == i)) {
                        browserSelected = i;
                    }
                    ImGui::PopID();
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
