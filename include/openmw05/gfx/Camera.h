#pragma once
// Minimal camera math for the M3 model viewer. Header-only, GL-free.
// Matrices are column-major float[16], ready for glUniformMatrix4fv with
// transpose = GL_FALSE. Hand-rolled to keep dependencies light (glm can
// replace this later if world rendering needs more).

#include <cmath>

namespace omw05 {
namespace gfx {

// out = a * b (column-major 4x4).
inline void mat4Multiply(const float a[16], const float b[16], float out[16]) {
    float r[16];
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r[c * 4 + row] = a[0 * 4 + row] * b[c * 4 + 0] + a[1 * 4 + row] * b[c * 4 + 1] +
                             a[2 * 4 + row] * b[c * 4 + 2] + a[3 * 4 + row] * b[c * 4 + 3];
        }
    }
    for (int i = 0; i < 16; ++i) {
        out[i] = r[i];
    }
}

inline void mat4Perspective(float fovyRadians, float aspect, float zNear, float zFar,
                            float out[16]) {
    const float f = 1.0f / std::tan(fovyRadians * 0.5f);
    for (int i = 0; i < 16; ++i) {
        out[i] = 0.0f;
    }
    out[0] = f / aspect;
    out[5] = f;
    out[10] = (zFar + zNear) / (zNear - zFar);
    out[11] = -1.0f;
    out[14] = (2.0f * zFar * zNear) / (zNear - zFar);
}

inline void mat4LookAt(const float eye[3], const float center[3], const float up[3],
                       float out[16]) {
    float f[3] = {center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]};
    const float fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    for (float& v : f) {
        v /= (fl > 0 ? fl : 1);
    }
    float s[3] = {f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2],
                  f[0] * up[1] - f[1] * up[0]};
    const float sl = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    for (float& v : s) {
        v /= (sl > 0 ? sl : 1);
    }
    const float u[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2],
                        s[0] * f[1] - s[1] * f[0]};
    out[0] = s[0];
    out[1] = u[0];
    out[2] = -f[0];
    out[3] = 0;
    out[4] = s[1];
    out[5] = u[1];
    out[6] = -f[1];
    out[7] = 0;
    out[8] = s[2];
    out[9] = u[2];
    out[10] = -f[2];
    out[11] = 0;
    out[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
    out[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    out[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
    out[15] = 1;
}

// Orbit camera around a target point. MW data is Z-up: pitch rotates toward
// +Z, the up vector is +Z.
struct OrbitCamera {
    float target[3] = {0, 0, 0};
    float distance = 5.0f;
    float yaw = 0.8f;    // radians around Z
    float pitch = 0.5f;  // radians above the XY plane

    void clampPitch() {
        const float limit = 1.55f;
        if (pitch > limit) {
            pitch = limit;
        }
        if (pitch < -limit) {
            pitch = -limit;
        }
    }

    void viewMatrix(float out[16]) const {
        const float cp = std::cos(pitch);
        const float eye[3] = {target[0] + distance * cp * std::cos(yaw),
                              target[1] + distance * cp * std::sin(yaw),
                              target[2] + distance * std::sin(pitch)};
        const float up[3] = {0, 0, 1};
        mat4LookAt(eye, target, up, out);
    }

    void viewProj(float aspect, float out[16]) const {
        float view[16];
        float proj[16];
        viewMatrix(view);
        mat4Perspective(0.9f, aspect, 0.05f, distance * 100.0f + 100.0f, proj);
        mat4Multiply(proj, view, out);
    }
};

} // namespace gfx
} // namespace omw05
