#include "openmw05/gfx/Shader.h"

#include <glad/gles2.h>

#include <string>

namespace omw05 {
namespace gfx {

namespace {
core::Result<GLuint> compileStage(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        return core::Error{core::ErrorCode::GraphicsError,
                           std::string(type == GL_VERTEX_SHADER ? "vertex" : "fragment") +
                               " shader compile failed: " + log};
    }
    return shader;
}
} // namespace

core::Result<Shader> Shader::compile(const char* vertexSrc, const char* fragmentSrc) {
    core::Result<GLuint> vs = compileStage(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) {
        return vs.error();
    }
    core::Result<GLuint> fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) {
        glDeleteShader(vs.value());
        return fs.error();
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs.value());
    glAttachShader(program, fs.value());
    glLinkProgram(program);
    glDeleteShader(vs.value());
    glDeleteShader(fs.value());

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        return core::Error{core::ErrorCode::GraphicsError,
                           std::string("program link failed: ") + log};
    }
    return Shader(program);
}

Shader::Shader(Shader&& other) noexcept : program_(other.program_) { other.program_ = 0; }

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (program_) {
            glDeleteProgram(program_);
        }
        program_ = other.program_;
        other.program_ = 0;
    }
    return *this;
}

Shader::~Shader() {
    if (program_) {
        glDeleteProgram(program_);
    }
}

void Shader::use() const { glUseProgram(program_); }

int Shader::uniformLocation(const char* name) const { return glGetUniformLocation(program_, name); }

} // namespace gfx
} // namespace omw05
