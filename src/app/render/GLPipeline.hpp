#pragma once

#include "ShaderLoom/Image.hpp"
#include "ShaderLoom/Processing.hpp"

#include <GLFW/glfw3.h>

#include <string>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace ShaderLoom::app {

enum class PreviewEffect {
    Passthrough,
    Ascii,
    Dither,
    Halftone,
    Dots,
    Contour
};

struct AsciiUniforms {
    float scale = 1.0F;
    float spacing = 0.3F;
    int outputWidth = 0;
    int characterSet = 3;
    GLuint glyphAtlasTexture = 0;
    int atlasColumns = 16;
    int atlasRows = 6;
};

struct DitherUniforms {
    int algorithm = 0;
    float intensity = 0.1F;
    bool modulation = false;
};

struct HalftoneUniforms {
    int shape = 0;
    float dotScale = 0.7F;
    float spacing = 7.0F;
    float angleDegrees = 15.0F;
    bool invert = true;
};

struct DotsUniforms {
    int shape = 0;
    int gridType = 0;
    float size = 1.3F;
    float spacing = 18.0F;
    bool invert = true;
};

struct ContourUniforms {
    float levels = 8.0F;
    float lineThickness = 1.0F;
    bool invert = false;
};

struct PreviewRenderSettings {
    PreviewEffect effect = PreviewEffect::Passthrough;
    RenderContext context;
    AsciiUniforms ascii;
    DitherUniforms dither;
    HalftoneUniforms halftone;
    DotsUniforms dots;
    ContourUniforms contour;
    bool sourceAlreadyProcessed = false;
    bool bloom = false;
    bool grain = false;
    bool chromatic = false;
    bool scanlines = false;
    bool vignette = false;
    bool crtCurve = false;
    bool phosphor = false;
    float bloomThreshold = 0.1F;
    float bloomSoftThreshold = 1.0F;
    float bloomIntensity = 0.7F;
    float bloomRadius = 7.0F;
    float grainIntensity = 35.0F;
    float grainSize = 2.0F;
    float grainSpeed = 50.0F;
    float chromaticAmount = 6.0F;
    float scanlineIntensity = 0.25F;
    float vignetteIntensity = 0.45F;
    float crtCurveAmount = 0.12F;
    float phosphorStrength = 0.35F;
    float timeSeconds = 0.0F;
};

class ShaderProgram {
public:
    ShaderProgram() = default;
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;
    ~ShaderProgram();

    void compile(const char* vertexSource, const char* fragmentSource);
    void use() const;
    void setInt(const char* name, int value) const;
    void setFloat(const char* name, float value) const;
    void setVec2(const char* name, float x, float y) const;
    [[nodiscard]] GLuint id() const noexcept;

private:
    GLuint program_ = 0;
};

class RenderTexture {
public:
    RenderTexture() = default;
    RenderTexture(const RenderTexture&) = delete;
    RenderTexture& operator=(const RenderTexture&) = delete;
    RenderTexture(RenderTexture&& other) noexcept;
    RenderTexture& operator=(RenderTexture&& other) noexcept;
    ~RenderTexture();

    void resize(int width, int height);
    void reset();

    [[nodiscard]] GLuint id() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;

private:
    GLuint texture_ = 0;
    int width_ = 0;
    int height_ = 0;
};

class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;
    ~Framebuffer();

    void attach(RenderTexture& texture);
    void bind() const;
    static void bindDefault();
    void reset();

private:
    GLuint framebuffer_ = 0;
};

class FullscreenQuad {
public:
    FullscreenQuad() = default;
    FullscreenQuad(const FullscreenQuad&) = delete;
    FullscreenQuad& operator=(const FullscreenQuad&) = delete;
    FullscreenQuad(FullscreenQuad&& other) noexcept;
    FullscreenQuad& operator=(FullscreenQuad&& other) noexcept;
    ~FullscreenQuad();

    void initialize();
    void draw() const;
    void reset();

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};

class PreviewPipeline {
public:
    void initialize();
    GLuint render(GLuint sourceTexture, int width, int height, const PreviewRenderSettings& settings);
    [[nodiscard]] Image readOutputImage() const;
    [[nodiscard]] bool hasOutput() const noexcept;
    void reset();

private:
    bool initialized_ = false;
    ShaderProgram preprocessShader_;
    ShaderProgram passthroughShader_;
    ShaderProgram asciiShader_;
    ShaderProgram ditherShader_;
    ShaderProgram halftoneShader_;
    ShaderProgram dotsShader_;
    ShaderProgram contourShader_;
    ShaderProgram postShader_;
    RenderTexture preprocessTexture_;
    Framebuffer preprocessFramebuffer_;
    RenderTexture effectTexture_;
    Framebuffer effectFramebuffer_;
    RenderTexture outputTexture_;
    Framebuffer outputFramebuffer_;
    FullscreenQuad quad_;
};

void loadOpenGLPipelineFunctions();

} // namespace ShaderLoom::app
