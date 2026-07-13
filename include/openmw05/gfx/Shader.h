#pragma once
// GLSL ES 3.00 shader program wrapper. Sources must start with
// "#version 300 es" (CLAUDE.md §5). RAII: owns the GL program object;
// requires a current GL context for construction and destruction. Move-only.

#include "openmw05/core/Result.h"

#include <cstdint>

namespace omw05 {
namespace gfx {

class Shader {
public:
    // Compiles + links; returns GraphicsError with the GL info log on failure.
    static core::Result<Shader> compile(const char* vertexSrc, const char* fragmentSrc);

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    ~Shader();

    void use() const;
    std::uint32_t id() const { return program_; }
    // -1 when the uniform is absent/optimized out (logged once by caller).
    int uniformLocation(const char* name) const;

private:
    explicit Shader(std::uint32_t program) : program_(program) {}
    std::uint32_t program_ = 0;
};

} // namespace gfx
} // namespace omw05
