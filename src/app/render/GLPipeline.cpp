#include "app/render/GLPipeline.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

namespace ShaderLoom::app {
namespace {

using GlCreateShader = GLuint(APIENTRY*)(GLenum);
using GlShaderSource = void(APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
using GlCompileShader = void(APIENTRY*)(GLuint);
using GlGetShaderiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GlGetShaderInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using GlDeleteShader = void(APIENTRY*)(GLuint);
using GlCreateProgram = GLuint(APIENTRY*)();
using GlAttachShader = void(APIENTRY*)(GLuint, GLuint);
using GlLinkProgram = void(APIENTRY*)(GLuint);
using GlGetProgramiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GlGetProgramInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using GlDeleteProgram = void(APIENTRY*)(GLuint);
using GlUseProgram = void(APIENTRY*)(GLuint);
using GlGetUniformLocation = GLint(APIENTRY*)(GLuint, const char*);
using GlUniform1i = void(APIENTRY*)(GLint, GLint);
using GlUniform1f = void(APIENTRY*)(GLint, GLfloat);
using GlUniform2f = void(APIENTRY*)(GLint, GLfloat, GLfloat);
using GlActiveTexture = void(APIENTRY*)(GLenum);
using GlGenFramebuffers = void(APIENTRY*)(GLsizei, GLuint*);
using GlBindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
using GlFramebufferTexture2D = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using GlCheckFramebufferStatus = GLenum(APIENTRY*)(GLenum);
using GlDeleteFramebuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using GlGenVertexArrays = void(APIENTRY*)(GLsizei, GLuint*);
using GlBindVertexArray = void(APIENTRY*)(GLuint);
using GlDeleteVertexArrays = void(APIENTRY*)(GLsizei, const GLuint*);
using GlGenBuffers = void(APIENTRY*)(GLsizei, GLuint*);
using GlBindBuffer = void(APIENTRY*)(GLenum, GLuint);
using GlBufferData = void(APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum);
using GlDeleteBuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using GlEnableVertexAttribArray = void(APIENTRY*)(GLuint);
using GlVertexAttribPointer = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

GlCreateShader glCreateShaderPtr = nullptr;
GlShaderSource glShaderSourcePtr = nullptr;
GlCompileShader glCompileShaderPtr = nullptr;
GlGetShaderiv glGetShaderivPtr = nullptr;
GlGetShaderInfoLog glGetShaderInfoLogPtr = nullptr;
GlDeleteShader glDeleteShaderPtr = nullptr;
GlCreateProgram glCreateProgramPtr = nullptr;
GlAttachShader glAttachShaderPtr = nullptr;
GlLinkProgram glLinkProgramPtr = nullptr;
GlGetProgramiv glGetProgramivPtr = nullptr;
GlGetProgramInfoLog glGetProgramInfoLogPtr = nullptr;
GlDeleteProgram glDeleteProgramPtr = nullptr;
GlUseProgram glUseProgramPtr = nullptr;
GlGetUniformLocation glGetUniformLocationPtr = nullptr;
GlUniform1i glUniform1iPtr = nullptr;
GlUniform1f glUniform1fPtr = nullptr;
GlUniform2f glUniform2fPtr = nullptr;
GlActiveTexture glActiveTexturePtr = nullptr;
GlGenFramebuffers glGenFramebuffersPtr = nullptr;
GlBindFramebuffer glBindFramebufferPtr = nullptr;
GlFramebufferTexture2D glFramebufferTexture2DPtr = nullptr;
GlCheckFramebufferStatus glCheckFramebufferStatusPtr = nullptr;
GlDeleteFramebuffers glDeleteFramebuffersPtr = nullptr;
GlGenVertexArrays glGenVertexArraysPtr = nullptr;
GlBindVertexArray glBindVertexArrayPtr = nullptr;
GlDeleteVertexArrays glDeleteVertexArraysPtr = nullptr;
GlGenBuffers glGenBuffersPtr = nullptr;
GlBindBuffer glBindBufferPtr = nullptr;
GlBufferData glBufferDataPtr = nullptr;
GlDeleteBuffers glDeleteBuffersPtr = nullptr;
GlEnableVertexAttribArray glEnableVertexAttribArrayPtr = nullptr;
GlVertexAttribPointer glVertexAttribPointerPtr = nullptr;

template <typename T>
T loadFunction(const char* name) {
    auto* function = reinterpret_cast<T>(glfwGetProcAddress(name));
    if (function == nullptr) {
        throw std::runtime_error(std::string("Missing OpenGL function: ") + name);
    }
    return function;
}

GLuint compileStage(GLenum type, const char* source) {
    const GLuint shader = glCreateShaderPtr(type);
    glShaderSourcePtr(shader, 1, &source, nullptr);
    glCompileShaderPtr(shader);

    GLint success = 0;
    glGetShaderivPtr(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderivPtr(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)));
    glGetShaderInfoLogPtr(shader, logLength, nullptr, log.data());
    glDeleteShaderPtr(shader);
    throw std::runtime_error(std::string("Shader compilation failed: ") + log.data());
}

constexpr const char* PassthroughVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

constexpr const char* PassthroughFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;

void main() {
    fragColor = texture(uSource, vTexCoord);
}
)";

constexpr const char* HalftoneFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;
uniform vec2 uResolution;
uniform float uDotScale;
uniform float uSpacing;
uniform float uAngle;
uniform int uInvert;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

mat2 rotate2d(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

void main() {
    vec2 pixel = vTexCoord * uResolution;
    vec2 center = uResolution * 0.5;
    vec2 rotated = rotate2d(uAngle) * (pixel - center) + center;
    float spacing = max(uSpacing, 2.0);
    vec2 cell = floor(rotated / spacing);
    vec2 local = fract(rotated / spacing) * 2.0 - 1.0;
    vec2 samplePixel = (cell + 0.5) * spacing;
    vec2 sampleUv = clamp((rotate2d(-uAngle) * (samplePixel - center) + center) / uResolution, 0.0, 1.0);
    vec4 source = texture(uSource, sampleUv);
    float tone = luma(source.rgb);
    if (uInvert == 1) {
        tone = 1.0 - tone;
    }
    float radius = clamp(uDotScale, 0.0, 1.5) * (1.0 - tone);
    float edge = smoothstep(radius, radius - 0.12, length(local));
    vec3 color = mix(vec3(0.0), source.rgb, edge);
    fragColor = vec4(color, source.a);
}
)";

constexpr const char* DotsFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;
uniform vec2 uResolution;
uniform float uSize;
uniform float uSpacing;
uniform int uInvert;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 pixel = vTexCoord * uResolution;
    float spacing = max(uSpacing, 3.0);
    vec2 cell = floor(pixel / spacing);
    vec2 local = fract(pixel / spacing) * 2.0 - 1.0;
    vec2 sampleUv = clamp(((cell + 0.5) * spacing) / uResolution, 0.0, 1.0);
    vec4 source = texture(uSource, sampleUv);
    float tone = luma(source.rgb);
    if (uInvert == 1) {
        tone = 1.0 - tone;
    }
    float radius = clamp(uSize, 0.0, 2.0) * 0.45 * (1.0 - tone);
    float mask = smoothstep(radius, radius - 0.08, length(local));
    fragColor = vec4(mix(vec3(0.0), source.rgb, mask), source.a);
}
)";

constexpr const char* ContourFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;
uniform vec2 uResolution;
uniform float uLevels;
uniform float uLineThickness;
uniform int uInvert;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float bandAt(vec2 uv, float levels) {
    float tone = luma(texture(uSource, clamp(uv, 0.0, 1.0)).rgb);
    if (uInvert == 1) {
        tone = 1.0 - tone;
    }
    return floor(tone * levels) / levels;
}

void main() {
    float levels = max(uLevels, 2.0);
    vec2 texel = max(uLineThickness, 0.5) / uResolution;
    vec4 source = texture(uSource, vTexCoord);
    float center = bandAt(vTexCoord, levels);
    float right = bandAt(vTexCoord + vec2(texel.x, 0.0), levels);
    float up = bandAt(vTexCoord + vec2(0.0, texel.y), levels);
    float line = step(0.001, abs(center - right) + abs(center - up));
    vec3 filled = floor(source.rgb * levels) / levels;
    if (uInvert == 1) {
        filled = 1.0 - filled;
    }
    fragColor = vec4(mix(filled, vec3(0.0), line), source.a);
}
)";

} // namespace

void loadOpenGLPipelineFunctions() {
    glCreateShaderPtr = loadFunction<GlCreateShader>("glCreateShader");
    glShaderSourcePtr = loadFunction<GlShaderSource>("glShaderSource");
    glCompileShaderPtr = loadFunction<GlCompileShader>("glCompileShader");
    glGetShaderivPtr = loadFunction<GlGetShaderiv>("glGetShaderiv");
    glGetShaderInfoLogPtr = loadFunction<GlGetShaderInfoLog>("glGetShaderInfoLog");
    glDeleteShaderPtr = loadFunction<GlDeleteShader>("glDeleteShader");
    glCreateProgramPtr = loadFunction<GlCreateProgram>("glCreateProgram");
    glAttachShaderPtr = loadFunction<GlAttachShader>("glAttachShader");
    glLinkProgramPtr = loadFunction<GlLinkProgram>("glLinkProgram");
    glGetProgramivPtr = loadFunction<GlGetProgramiv>("glGetProgramiv");
    glGetProgramInfoLogPtr = loadFunction<GlGetProgramInfoLog>("glGetProgramInfoLog");
    glDeleteProgramPtr = loadFunction<GlDeleteProgram>("glDeleteProgram");
    glUseProgramPtr = loadFunction<GlUseProgram>("glUseProgram");
    glGetUniformLocationPtr = loadFunction<GlGetUniformLocation>("glGetUniformLocation");
    glUniform1iPtr = loadFunction<GlUniform1i>("glUniform1i");
    glUniform1fPtr = loadFunction<GlUniform1f>("glUniform1f");
    glUniform2fPtr = loadFunction<GlUniform2f>("glUniform2f");
    glActiveTexturePtr = loadFunction<GlActiveTexture>("glActiveTexture");
    glGenFramebuffersPtr = loadFunction<GlGenFramebuffers>("glGenFramebuffers");
    glBindFramebufferPtr = loadFunction<GlBindFramebuffer>("glBindFramebuffer");
    glFramebufferTexture2DPtr = loadFunction<GlFramebufferTexture2D>("glFramebufferTexture2D");
    glCheckFramebufferStatusPtr = loadFunction<GlCheckFramebufferStatus>("glCheckFramebufferStatus");
    glDeleteFramebuffersPtr = loadFunction<GlDeleteFramebuffers>("glDeleteFramebuffers");
    glGenVertexArraysPtr = loadFunction<GlGenVertexArrays>("glGenVertexArrays");
    glBindVertexArrayPtr = loadFunction<GlBindVertexArray>("glBindVertexArray");
    glDeleteVertexArraysPtr = loadFunction<GlDeleteVertexArrays>("glDeleteVertexArrays");
    glGenBuffersPtr = loadFunction<GlGenBuffers>("glGenBuffers");
    glBindBufferPtr = loadFunction<GlBindBuffer>("glBindBuffer");
    glBufferDataPtr = loadFunction<GlBufferData>("glBufferData");
    glDeleteBuffersPtr = loadFunction<GlDeleteBuffers>("glDeleteBuffers");
    glEnableVertexAttribArrayPtr = loadFunction<GlEnableVertexAttribArray>("glEnableVertexAttribArray");
    glVertexAttribPointerPtr = loadFunction<GlVertexAttribPointer>("glVertexAttribPointer");
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : program_(std::exchange(other.program_, 0)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        if (program_ != 0) {
            glDeleteProgramPtr(program_);
        }
        program_ = std::exchange(other.program_, 0);
    }
    return *this;
}

ShaderProgram::~ShaderProgram() {
    if (program_ != 0) {
        glDeleteProgramPtr(program_);
    }
}

void ShaderProgram::compile(const char* vertexSource, const char* fragmentSource) {
    const GLuint vertexShader = compileStage(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = compileStage(GL_FRAGMENT_SHADER, fragmentSource);
    const GLuint program = glCreateProgramPtr();
    glAttachShaderPtr(program, vertexShader);
    glAttachShaderPtr(program, fragmentShader);
    glLinkProgramPtr(program);
    glDeleteShaderPtr(vertexShader);
    glDeleteShaderPtr(fragmentShader);

    GLint success = 0;
    glGetProgramivPtr(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramivPtr(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)));
        glGetProgramInfoLogPtr(program, logLength, nullptr, log.data());
        glDeleteProgramPtr(program);
        throw std::runtime_error(std::string("Shader link failed: ") + log.data());
    }

    if (program_ != 0) {
        glDeleteProgramPtr(program_);
    }
    program_ = program;
}

void ShaderProgram::use() const {
    glUseProgramPtr(program_);
}

void ShaderProgram::setInt(const char* name, int value) const {
    const GLint location = glGetUniformLocationPtr(program_, name);
    if (location >= 0) {
        glUniform1iPtr(location, value);
    }
}

void ShaderProgram::setFloat(const char* name, float value) const {
    const GLint location = glGetUniformLocationPtr(program_, name);
    if (location >= 0) {
        glUniform1fPtr(location, value);
    }
}

void ShaderProgram::setVec2(const char* name, float x, float y) const {
    const GLint location = glGetUniformLocationPtr(program_, name);
    if (location >= 0) {
        glUniform2fPtr(location, x, y);
    }
}

GLuint ShaderProgram::id() const noexcept {
    return program_;
}

RenderTexture::RenderTexture(RenderTexture&& other) noexcept
    : texture_(std::exchange(other.texture_, 0)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)) {}

RenderTexture& RenderTexture::operator=(RenderTexture&& other) noexcept {
    if (this != &other) {
        reset();
        texture_ = std::exchange(other.texture_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

RenderTexture::~RenderTexture() {
    reset();
}

void RenderTexture::resize(int width, int height) {
    if (texture_ != 0 && width_ == width && height_ == height) {
        return;
    }

    if (texture_ == 0) {
        glGenTextures(1, &texture_);
    }
    width_ = width;
    height_ = height;

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderTexture::reset() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

GLuint RenderTexture::id() const noexcept {
    return texture_;
}

int RenderTexture::width() const noexcept {
    return width_;
}

int RenderTexture::height() const noexcept {
    return height_;
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : framebuffer_(std::exchange(other.framebuffer_, 0)) {}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        reset();
        framebuffer_ = std::exchange(other.framebuffer_, 0);
    }
    return *this;
}

Framebuffer::~Framebuffer() {
    reset();
}

void Framebuffer::attach(RenderTexture& texture) {
    if (framebuffer_ == 0) {
        glGenFramebuffersPtr(1, &framebuffer_);
    }

    glBindFramebufferPtr(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2DPtr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.id(), 0);
    if (glCheckFramebufferStatusPtr(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebufferPtr(GL_FRAMEBUFFER, 0);
        throw std::runtime_error("Framebuffer is incomplete.");
    }
    glBindFramebufferPtr(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bind() const {
    glBindFramebufferPtr(GL_FRAMEBUFFER, framebuffer_);
}

void Framebuffer::bindDefault() {
    glBindFramebufferPtr(GL_FRAMEBUFFER, 0);
}

void Framebuffer::reset() {
    if (framebuffer_ != 0) {
        glDeleteFramebuffersPtr(1, &framebuffer_);
        framebuffer_ = 0;
    }
}

FullscreenQuad::FullscreenQuad(FullscreenQuad&& other) noexcept
    : vao_(std::exchange(other.vao_, 0)),
      vbo_(std::exchange(other.vbo_, 0)) {}

FullscreenQuad& FullscreenQuad::operator=(FullscreenQuad&& other) noexcept {
    if (this != &other) {
        reset();
        vao_ = std::exchange(other.vao_, 0);
        vbo_ = std::exchange(other.vbo_, 0);
    }
    return *this;
}

FullscreenQuad::~FullscreenQuad() {
    reset();
}

void FullscreenQuad::initialize() {
    if (vao_ != 0) {
        return;
    }

    constexpr std::array<float, 24> vertices = {
        -1.0F, -1.0F, 0.0F, 0.0F,
         1.0F, -1.0F, 1.0F, 0.0F,
         1.0F,  1.0F, 1.0F, 1.0F,
        -1.0F, -1.0F, 0.0F, 0.0F,
         1.0F,  1.0F, 1.0F, 1.0F,
        -1.0F,  1.0F, 0.0F, 1.0F
    };

    glGenVertexArraysPtr(1, &vao_);
    glGenBuffersPtr(1, &vbo_);
    glBindVertexArrayPtr(vao_);
    glBindBufferPtr(GL_ARRAY_BUFFER, vbo_);
    glBufferDataPtr(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArrayPtr(0);
    glVertexAttribPointerPtr(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArrayPtr(1);
    glVertexAttribPointerPtr(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindBufferPtr(GL_ARRAY_BUFFER, 0);
    glBindVertexArrayPtr(0);
}

void FullscreenQuad::draw() const {
    glBindVertexArrayPtr(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArrayPtr(0);
}

void FullscreenQuad::reset() {
    if (vbo_ != 0) {
        glDeleteBuffersPtr(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArraysPtr(1, &vao_);
        vao_ = 0;
    }
}

void PreviewPipeline::initialize() {
    if (initialized_) {
        return;
    }

    passthroughShader_.compile(PassthroughVertexShader, PassthroughFragmentShader);
    halftoneShader_.compile(PassthroughVertexShader, HalftoneFragmentShader);
    dotsShader_.compile(PassthroughVertexShader, DotsFragmentShader);
    contourShader_.compile(PassthroughVertexShader, ContourFragmentShader);
    quad_.initialize();
    initialized_ = true;
}

GLuint PreviewPipeline::render(GLuint sourceTexture, int width, int height, const PreviewRenderSettings& settings) {
    initialize();
    outputTexture_.resize(width, height);
    outputFramebuffer_.attach(outputTexture_);

    outputFramebuffer_.bind();
    glViewport(0, 0, width, height);
    glDisable(GL_BLEND);
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    ShaderProgram* shader = &passthroughShader_;
    if (settings.effect == PreviewEffect::Halftone) {
        shader = &halftoneShader_;
    } else if (settings.effect == PreviewEffect::Dots) {
        shader = &dotsShader_;
    } else if (settings.effect == PreviewEffect::Contour) {
        shader = &contourShader_;
    }

    shader->use();
    glActiveTexturePtr(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    shader->setInt("uSource", 0);
    shader->setVec2("uResolution", static_cast<float>(width), static_cast<float>(height));

    if (settings.effect == PreviewEffect::Halftone) {
        shader->setFloat("uDotScale", settings.halftone.dotScale);
        shader->setFloat("uSpacing", settings.halftone.spacing);
        shader->setFloat("uAngle", settings.halftone.angleDegrees * 0.017453292519943295F);
        shader->setInt("uInvert", settings.halftone.invert ? 1 : 0);
    } else if (settings.effect == PreviewEffect::Dots) {
        shader->setFloat("uSize", settings.dots.size);
        shader->setFloat("uSpacing", settings.dots.spacing);
        shader->setInt("uInvert", settings.dots.invert ? 1 : 0);
    } else if (settings.effect == PreviewEffect::Contour) {
        shader->setFloat("uLevels", settings.contour.levels);
        shader->setFloat("uLineThickness", settings.contour.lineThickness);
        shader->setInt("uInvert", settings.contour.invert ? 1 : 0);
    }

    quad_.draw();
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgramPtr(0);
    Framebuffer::bindDefault();

    return outputTexture_.id();
}

Image PreviewPipeline::readOutputImage() const {
    if (!hasOutput()) {
        throw std::runtime_error("No rendered output is available to export.");
    }

    outputFramebuffer_.bind();
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(outputTexture_.width() * outputTexture_.height() * 4));
    glReadPixels(0, 0, outputTexture_.width(), outputTexture_.height(), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    Framebuffer::bindDefault();

    const int stride = outputTexture_.width() * 4;
    std::vector<std::uint8_t> flipped(pixels.size());
    for (int y = 0; y < outputTexture_.height(); ++y) {
        const std::size_t sourceOffset = static_cast<std::size_t>((outputTexture_.height() - 1 - y) * stride);
        const std::size_t targetOffset = static_cast<std::size_t>(y * stride);
        std::copy_n(pixels.data() + sourceOffset, static_cast<std::size_t>(stride), flipped.data() + targetOffset);
    }

    return Image(outputTexture_.width(), outputTexture_.height(), std::move(flipped));
}

bool PreviewPipeline::hasOutput() const noexcept {
    return outputTexture_.id() != 0 && outputTexture_.width() > 0 && outputTexture_.height() > 0;
}

void PreviewPipeline::reset() {
    outputFramebuffer_.reset();
    outputTexture_.reset();
    quad_.reset();
    passthroughShader_ = ShaderProgram();
    halftoneShader_ = ShaderProgram();
    dotsShader_ = ShaderProgram();
    contourShader_ = ShaderProgram();
    initialized_ = false;
}

} // namespace ShaderLoom::app
