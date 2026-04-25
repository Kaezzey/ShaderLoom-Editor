#pragma once

#include <GLFW/glfw3.h>

#include <string>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace ShaderLoom::app {

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
    GLuint render(GLuint sourceTexture, int width, int height);
    void reset();

private:
    bool initialized_ = false;
    ShaderProgram passthroughShader_;
    RenderTexture outputTexture_;
    Framebuffer outputFramebuffer_;
    FullscreenQuad quad_;
};

void loadOpenGLPipelineFunctions();

} // namespace ShaderLoom::app
