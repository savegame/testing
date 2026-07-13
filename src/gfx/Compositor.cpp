#include "openmw05/gfx/Compositor.h"

#include "openmw05/gfx/RenderTarget.h"

#include <glad/gles2.h>

namespace omw05 {
namespace gfx {

namespace {

// Fullscreen triangle from gl_VertexID — no vertex buffer needed in ES 3.0.
const char* kVertexSrc = R"(#version 300 es
precision highp float;
out vec2 vScreenUv;  // bottom-left origin, [0,1] over the drawable
void main() {
    vec2 pos = vec2(float((gl_VertexID & 1) << 2) - 1.0,
                    float((gl_VertexID & 2) << 1) - 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
    vScreenUv = pos * 0.5 + 0.5;
}
)";

// Rotation is a pure UV transform (CLAUDE.md §5a). Mapping is derived in
// top-left-origin normalized space: screen (u,v) -> logical (u',v'):
//   0:   (u, v)          90:  (v, 1-u)
//   180: (1-u, 1-v)      270: (1-v, u)
// uiRT content is effectively premultiplied (ImGui blends SRC_ALPHA /
// ONE_MINUS_SRC_ALPHA onto a transparent-black clear), hence the
// scene*(1-a) + ui composite. Post-processing hook slots in between later.
const char* kFragmentSrc = R"(#version 300 es
precision highp float;
in vec2 vScreenUv;
uniform sampler2D uScene;
uniform sampler2D uUi;
uniform int uRotation;  // 0..3 = 0/90/180/270 degrees
out vec4 oColor;
void main() {
    vec2 tl = vec2(vScreenUv.x, 1.0 - vScreenUv.y);
    vec2 l;
    if (uRotation == 1) {
        l = vec2(tl.y, 1.0 - tl.x);
    } else if (uRotation == 2) {
        l = vec2(1.0 - tl.x, 1.0 - tl.y);
    } else if (uRotation == 3) {
        l = vec2(1.0 - tl.y, tl.x);
    } else {
        l = tl;
    }
    vec2 st = vec2(l.x, 1.0 - l.y);
    vec4 scene = texture(uScene, st);
    vec4 ui = texture(uUi, st);
    oColor = vec4(scene.rgb * (1.0 - ui.a) + ui.rgb, 1.0);
}
)";

int rotationIndex(Rotation r) {
    switch (r) {
    case Rotation::Deg0:
        return 0;
    case Rotation::Deg90:
        return 1;
    case Rotation::Deg180:
        return 2;
    case Rotation::Deg270:
        return 3;
    }
    return 0;
}

} // namespace

bool rotationFromDegrees(int degrees, Rotation* out) {
    switch (degrees) {
    case 0:
        *out = Rotation::Deg0;
        return true;
    case 90:
        *out = Rotation::Deg90;
        return true;
    case 180:
        *out = Rotation::Deg180;
        return true;
    case 270:
        *out = Rotation::Deg270;
        return true;
    default:
        return false;
    }
}

void logicalSize(Rotation rotation, int physicalW, int physicalH, int* logicalW, int* logicalH) {
    if (rotation == Rotation::Deg90 || rotation == Rotation::Deg270) {
        *logicalW = physicalH;
        *logicalH = physicalW;
    } else {
        *logicalW = physicalW;
        *logicalH = physicalH;
    }
}

core::Result<Compositor> Compositor::create() {
    core::Result<Shader> shader = Shader::compile(kVertexSrc, kFragmentSrc);
    if (!shader) {
        return shader.error();
    }
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    return core::Result<Compositor>(Compositor(shader.take(), vao));
}

Compositor::Compositor(Shader shader, std::uint32_t vao) : shader_(std::move(shader)), vao_(vao) {
    locScene_ = shader_.uniformLocation("uScene");
    locUi_ = shader_.uniformLocation("uUi");
    locRotation_ = shader_.uniformLocation("uRotation");
}

Compositor::Compositor(Compositor&& other) noexcept
    : shader_(std::move(other.shader_)), vao_(other.vao_), locScene_(other.locScene_),
      locUi_(other.locUi_), locRotation_(other.locRotation_) {
    other.vao_ = 0;
}

Compositor& Compositor::operator=(Compositor&& other) noexcept {
    if (this != &other) {
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
        }
        shader_ = std::move(other.shader_);
        vao_ = other.vao_;
        locScene_ = other.locScene_;
        locUi_ = other.locUi_;
        locRotation_ = other.locRotation_;
        other.vao_ = 0;
    }
    return *this;
}

Compositor::~Compositor() {
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
    }
}

void Compositor::composite(const RenderTarget& scene, const RenderTarget& ui, Rotation rotation,
                           int physicalW, int physicalH) const {
    RenderTarget::bindDefault();
    glViewport(0, 0, physicalW, physicalH);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    shader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene.colorTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, ui.colorTexture());
    glUniform1i(locScene_, 0);
    glUniform1i(locUi_, 1);
    glUniform1i(locRotation_, rotationIndex(rotation));

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Compositor::physicalToLogical(Rotation rotation, int physicalW, int physicalH, float px,
                                   float py, float* lx, float* ly) {
    const float u = px / static_cast<float>(physicalW);
    const float v = py / static_cast<float>(physicalH);
    float lu;
    float lv;
    switch (rotation) {  // same mapping as the composite shader, top-left space
    case Rotation::Deg90:
        lu = v;
        lv = 1.0f - u;
        break;
    case Rotation::Deg180:
        lu = 1.0f - u;
        lv = 1.0f - v;
        break;
    case Rotation::Deg270:
        lu = 1.0f - v;
        lv = u;
        break;
    case Rotation::Deg0:
    default:
        lu = u;
        lv = v;
        break;
    }
    int lw;
    int lh;
    logicalSize(rotation, physicalW, physicalH, &lw, &lh);
    *lx = lu * static_cast<float>(lw);
    *ly = lv * static_cast<float>(lh);
}

} // namespace gfx
} // namespace omw05
