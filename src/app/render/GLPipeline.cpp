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
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
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

constexpr const char* PreprocessFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;
uniform vec2 uResolution;
uniform int uInvert;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uHueRotation;
uniform float uSharpness;
uniform float uGamma;
uniform float uBrightnessMap;
uniform float uEdgeEnhance;
uniform float uBlur;
uniform float uQuantizeColors;
uniform float uShapeMatching;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

mat3 hueRotationMatrix(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        0.213 + c * 0.787 - s * 0.213, 0.715 - c * 0.715 - s * 0.715, 0.072 - c * 0.072 + s * 0.928,
        0.213 - c * 0.213 + s * 0.143, 0.715 + c * 0.285 + s * 0.140, 0.072 - c * 0.072 - s * 0.283,
        0.213 - c * 0.213 - s * 0.787, 0.715 - c * 0.715 + s * 0.715, 0.072 + c * 0.928 + s * 0.072
    );
}

vec3 blurSample(vec2 uv, vec2 texel) {
    vec3 sum = texture(uSource, uv).rgb * 4.0;
    sum += texture(uSource, uv + vec2(texel.x, 0.0)).rgb;
    sum += texture(uSource, uv - vec2(texel.x, 0.0)).rgb;
    sum += texture(uSource, uv + vec2(0.0, texel.y)).rgb;
    sum += texture(uSource, uv - vec2(0.0, texel.y)).rgb;
    sum += texture(uSource, uv + texel).rgb;
    sum += texture(uSource, uv - texel).rgb;
    sum += texture(uSource, uv + vec2(texel.x, -texel.y)).rgb;
    sum += texture(uSource, uv + vec2(-texel.x, texel.y)).rgb;
    return sum / 12.0;
}

float sobel(vec2 uv, vec2 texel) {
    float tl = luma(texture(uSource, uv + texel * vec2(-1.0, -1.0)).rgb);
    float tc = luma(texture(uSource, uv + texel * vec2(0.0, -1.0)).rgb);
    float tr = luma(texture(uSource, uv + texel * vec2(1.0, -1.0)).rgb);
    float ml = luma(texture(uSource, uv + texel * vec2(-1.0, 0.0)).rgb);
    float mr = luma(texture(uSource, uv + texel * vec2(1.0, 0.0)).rgb);
    float bl = luma(texture(uSource, uv + texel * vec2(-1.0, 1.0)).rgb);
    float bc = luma(texture(uSource, uv + texel * vec2(0.0, 1.0)).rgb);
    float br = luma(texture(uSource, uv + texel * vec2(1.0, 1.0)).rgb);
    float gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    float gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;
    return clamp(length(vec2(gx, gy)), 0.0, 1.0);
}

void main() {
    vec2 texel = 1.0 / max(uResolution, vec2(1.0));
    vec4 source = texture(uSource, vTexCoord);
    vec3 color = source.rgb;

    if (uBlur > 0.001) {
        color = mix(color, blurSample(vTexCoord, texel * max(uBlur, 1.0)), clamp(uBlur / 10.0, 0.0, 1.0));
    }

    if (uInvert == 1) {
        color = 1.0 - color;
    }

    color += uBrightness / 100.0;
    color = (color - 0.5) * (1.0 + uContrast / 100.0) + 0.5;
    float gray = luma(color);
    color = mix(vec3(gray), color, 1.0 + uSaturation / 100.0);
    color = hueRotationMatrix(radians(uHueRotation)) * color;
    color = clamp(color, vec3(0.0), vec3(1.0));

    if (uSharpness > 0.001) {
        vec3 center = texture(uSource, vTexCoord).rgb;
        vec3 sharp = center * 5.0
            - texture(uSource, vTexCoord + vec2(texel.x, 0.0)).rgb
            - texture(uSource, vTexCoord - vec2(texel.x, 0.0)).rgb
            - texture(uSource, vTexCoord + vec2(0.0, texel.y)).rgb
            - texture(uSource, vTexCoord - vec2(0.0, texel.y)).rgb;
        color = mix(color, sharp, clamp(uSharpness / 5.0, 0.0, 1.0));
    }

    color = pow(clamp(color, vec3(0.0), vec3(1.0)), vec3(1.0 / max(uGamma, 0.01)));

    float mappedLuma = max(luma(color), 0.001);
    float targetLuma = pow(mappedLuma, max(uBrightnessMap, 0.01));
    color *= targetLuma / mappedLuma;

    if (uShapeMatching > 0.001) {
        float currentLuma = max(luma(color), 0.001);
        float shapedLuma = floor(smoothstep(0.05, 0.95, currentLuma) * 5.0 + 0.5) / 5.0;
        color = mix(color, color * (shapedLuma / currentLuma), clamp(uShapeMatching, 0.0, 1.0));
    }

    if (uEdgeEnhance > 0.001) {
        color += sobel(vTexCoord, texel) * (uEdgeEnhance / 5.0);
    }

    if (uQuantizeColors > 1.0) {
        float levels = max(uQuantizeColors - 1.0, 1.0);
        color = floor(clamp(color, vec3(0.0), vec3(1.0)) * levels + 0.5) / levels;
    }

    fragColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), source.a);
}
)";

constexpr const char* AsciiFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;
uniform sampler2D uGlyphAtlas;
uniform vec2 uResolution;
uniform float uScale;
uniform float uSpacing;
uniform int uOutputWidth;
uniform int uCharacterSet;
uniform int uAtlasColumns;
uniform int uAtlasRows;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

int rampCount(int set) {
    if (set == 0) { return 10; }
    if (set == 1) { return 5; }
    if (set == 2) { return 3; }
    if (set == 3) { return 67; }
    if (set == 4) { return 10; }
    if (set == 5) { return 36; }
    if (set == 6) { return 11; }
    if (set == 7) { return 10; }
    if (set == 8) { return 22; }
    return 10;
}

int asciiTile(int code) {
    return clamp(code - 32, 0, 94);
}

int rampTile(int set, int index) {
    if (set == 0) {
        int data[10] = int[10](32,46,58,45,61,43,42,35,37,64);
        return asciiTile(data[clamp(index, 0, 9)]);
    }
    if (set == 1) {
        int data[5] = int[5](0,95,96,97,98);
        return data[clamp(index, 0, 4)];
    }
    if (set == 2) {
        int data[3] = int[3](32,48,49);
        return asciiTile(data[clamp(index, 1, 2)]);
    }
    if (set == 3) {
        int data[67] = int[67](32,46,94,34,44,58,59,73,108,33,105,62,60,126,43,45,63,93,91,125,123,49,41,40,124,92,47,116,102,106,114,120,110,117,118,99,122,88,89,85,74,67,76,81,48,79,90,109,119,113,112,100,98,107,104,97,111,42,35,77,87,38,56,37,66,64,36);
        return asciiTile(data[clamp(index, 0, 66)]);
    }
    if (set == 4) {
        int data[10] = int[10](32,46,58,45,61,43,42,35,37,64);
        return asciiTile(data[clamp(index, 0, 9)]);
    }
    if (set == 5) {
        int data[36] = int[36](32,46,44,58,105,108,99,118,117,110,120,114,106,102,116,76,67,74,85,89,88,90,79,48,81,100,98,112,113,119,109,104,97,111,77,87);
        return asciiTile(data[clamp(index, 0, 35)]);
    }
    if (set == 6) {
        int data[11] = int[11](32,49,50,51,52,53,54,55,56,57,48);
        return asciiTile(data[clamp(index, 0, 10)]);
    }
    if (set == 7) {
        int data[10] = int[10](32,46,45,43,61,42,47,37,35,64);
        return asciiTile(data[clamp(index, 0, 9)]);
    }
    if (set == 8) {
        int data[22] = int[22](32,46,44,58,59,33,60,62,63,47,124,92,123,125,91,93,40,41,35,36,37,64);
        return asciiTile(data[clamp(index, 0, 21)]);
    }
    int data[10] = int[10](32,46,58,45,61,43,42,35,37,64);
    return asciiTile(data[clamp(index, 0, 9)]);
}

void main() {
    float cellWidth = max(4.0, 8.0 / max(uScale, 0.1));
    float columns = uOutputWidth > 0 ? float(uOutputWidth) : clamp(floor(uResolution.x / cellWidth + 0.5), 12.0, 900.0);
    vec2 cellSize = vec2(uResolution.x / columns);
    cellSize.y = cellSize.x * 1.28;
    float rows = max(1.0, floor(uResolution.y / max(cellSize.y, 1.0) + 0.5));
    cellSize.y = uResolution.y / rows;

    vec2 pixel = vTexCoord * uResolution;
    vec2 cell = floor(pixel / cellSize);
    vec2 cellOrigin = cell * cellSize;
    vec2 local = (pixel - cellOrigin) / cellSize;
    vec2 sampleUv = clamp((cellOrigin + cellSize * 0.5) / uResolution, vec2(0.0), vec2(1.0));
    vec4 source = texture(uSource, sampleUv);
    float sourceTone = luma(source.rgb);

    int count = rampCount(uCharacterSet);
    int rampIndex = int(clamp(floor(sourceTone * float(count - 1) + 0.5), 0.0, float(count - 1)));
    if ((uCharacterSet == 2 || uCharacterSet == 3) && rampIndex == 0) {
        rampIndex = 1;
    }
    int tile = clamp(rampTile(uCharacterSet, rampIndex), 0, (uAtlasColumns * uAtlasRows) - 1);

    float glyphScale = clamp(1.18 - (uSpacing * 0.24), 0.38, 1.22);
    vec2 glyphUv = ((local - 0.5) / glyphScale) + 0.5;
    if (glyphUv.x < 0.0 || glyphUv.y < 0.0 || glyphUv.x > 1.0 || glyphUv.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, source.a);
        return;
    }

    float atlasColumns = float(max(uAtlasColumns, 1));
    float atlasRows = float(max(uAtlasRows, 1));
    vec2 tileUv = vec2(mod(float(tile), atlasColumns), floor(float(tile) / atlasColumns));
    vec2 atlasGrid = vec2(atlasColumns, atlasRows);
    vec2 tileSize = 1.0 / atlasGrid;
    vec2 atlasTexel = 1.0 / vec2(textureSize(uGlyphAtlas, 0));
    vec2 atlasUv = tileUv * tileSize + atlasTexel * 0.5 + glyphUv * max(tileSize - atlasTexel, vec2(0.0));
    float distanceValue = texture(uGlyphAtlas, atlasUv).a;
    float edgeWidth = max(fwidth(distanceValue) * 1.5, 0.003);
    float glyphAlpha = smoothstep(0.5 - edgeWidth, 0.5 + edgeWidth, distanceValue);
    fragColor = vec4(source.rgb * glyphAlpha, source.a);
}
)";

constexpr const char* DitherFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;
uniform vec2 uResolution;
uniform int uAlgorithm;
uniform float uIntensity;
uniform int uModulation;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float bayer2(vec2 p) {
    int x = int(mod(p.x, 2.0));
    int y = int(mod(p.y, 2.0));
    if (x == 0 && y == 0) { return 0.25; }
    if (x == 1 && y == 0) { return 0.75; }
    if (x == 0 && y == 1) { return 1.0; }
    return 0.5;
}

float bayer4(vec2 p) {
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    int index = x + y * 4;
    int data[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
    return (float(data[index]) + 0.5) / 16.0;
}

float orderedThreshold(vec2 p, int algorithm) {
    if (algorithm == 8) {
        return bayer2(p);
    }
    if (algorithm == 1) {
        return mix(bayer4(p), hash(floor(p / 2.0)), 0.18);
    }
    if (algorithm == 2 || algorithm == 3) {
        return mix(bayer4(p * 0.5), hash(floor(p)), 0.28);
    }
    if (algorithm == 4 || algorithm == 5) {
        return mix(bayer4(p + vec2(p.y * 0.5, 0.0)), hash(floor(p * 0.7)), 0.18);
    }
    if (algorithm == 6 || algorithm == 7) {
        return mix(bayer2(p + vec2(p.y, 0.0)), hash(floor(p)), 0.12);
    }
    return mix(bayer4(p), hash(floor(p + vec2(p.y * 0.37, 0.0))), 0.22);
}

void main() {
    vec2 pixel = vTexCoord * uResolution;
    vec4 source = texture(uSource, vTexCoord);
    float threshold = orderedThreshold(pixel, uAlgorithm);
    float strength = clamp(uIntensity, 0.0, 1.0);
    float tone = luma(source.rgb);
    if (uModulation == 1) {
        float chroma = length(source.rgb - vec3(tone));
        threshold = clamp(threshold + (hash(floor(pixel * 0.5)) - 0.5) * chroma * 0.35, 0.0, 1.0);
    }
    float bias = mix(0.08, -0.08, strength);
    float mask = step(threshold, clamp(tone + bias, 0.0, 1.0));
    vec3 dithered = source.rgb * mask;
    float amount = mix(0.70, 1.0, strength);
    fragColor = vec4(mix(source.rgb, dithered, amount), source.a);
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
uniform int uShape;
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
    vec2 sampleUv = clamp((rotate2d(-uAngle) * (samplePixel - center) + center) / uResolution, vec2(0.0), vec2(1.0));
    vec4 source = texture(uSource, sampleUv);
    float tone = luma(source.rgb);
    if (uInvert == 1) {
        tone = 1.0 - tone;
    }
    float dotScale = clamp(uDotScale, 0.0, 1.5);
    float radius = max(dotScale * (1.0 - tone), dotScale * 0.045);
    float distanceValue = length(local);
    if (uShape == 1) {
        distanceValue = max(abs(local.x), abs(local.y));
    } else if (uShape == 2) {
        distanceValue = abs(local.x) + abs(local.y);
    } else if (uShape == 3) {
        distanceValue = abs(local.y);
    }
    float edge = 1.0 - smoothstep(max(radius - 0.12, 0.0), radius, distanceValue);
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
uniform int uShape;
uniform int uGridType;
uniform int uInvert;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 pixel = vTexCoord * uResolution;
    float spacing = max(uSpacing, 3.0);
    if (uGridType == 1) {
        float row = floor(pixel.y / spacing);
        pixel.x += mod(row, 2.0) * spacing * 0.5;
    }
    vec2 cell = floor(pixel / spacing);
    vec2 local = fract(pixel / spacing) * 2.0 - 1.0;
    vec2 sampleUv = clamp(((cell + 0.5) * spacing) / uResolution, vec2(0.0), vec2(1.0));
    vec4 source = texture(uSource, sampleUv);
    float tone = luma(source.rgb);
    if (uInvert == 1) {
        tone = 1.0 - tone;
    }
    float dotSize = clamp(uSize, 0.0, 2.0) * 0.45;
    float radius = max(dotSize * (1.0 - tone), dotSize * 0.055);
    float distanceValue = length(local);
    if (uShape == 1) {
        distanceValue = max(abs(local.x), abs(local.y));
    } else if (uShape == 2) {
        distanceValue = abs(local.x) + abs(local.y);
    }
    float mask = 1.0 - smoothstep(max(radius - 0.08, 0.0), radius, distanceValue);
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
    float tone = luma(texture(uSource, clamp(uv, vec2(0.0), vec2(1.0))).rgb);
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

constexpr const char* PixelSortFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;
uniform vec2 uResolution;
uniform vec2 uTextureResolution;
uniform int uDirection;
uniform int uSortMode;
uniform float uThreshold;
uniform int uStreakLength;
uniform float uIntensity;
uniform float uRandomness;
uniform int uReverse;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float saturation(vec3 color) {
    float hi = max(max(color.r, color.g), color.b);
    float lo = min(min(color.r, color.g), color.b);
    return hi <= 0.0001 ? 0.0 : (hi - lo) / hi;
}

float hue(vec3 color) {
    float hi = max(max(color.r, color.g), color.b);
    float lo = min(min(color.r, color.g), color.b);
    float delta = hi - lo;
    if (delta <= 0.0001) {
        return 0.0;
    }
    float h = 0.0;
    if (hi == color.r) {
        h = mod((color.g - color.b) / delta, 6.0);
    } else if (hi == color.g) {
        h = ((color.b - color.r) / delta) + 2.0;
    } else {
        h = ((color.r - color.g) / delta) + 4.0;
    }
    h /= 6.0;
    return h < 0.0 ? h + 1.0 : h;
}

float sortMetric(vec3 color) {
    if (uSortMode == 1) {
        return hue(color);
    }
    if (uSortMode == 2) {
        return saturation(color);
    }
    return luma(color);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec4 source = texture(uSource, vTexCoord);
    float metric = sortMetric(source.rgb);
    vec2 direction = vec2(1.0, 0.0);
    if (uDirection == 1) {
        direction = vec2(0.0, 1.0);
    } else if (uDirection == 2) {
        direction = normalize(vec2(1.0, 1.0));
    }
    if (uReverse == 1) {
        direction = -direction;
    }

    vec2 logicalResolution = max(uResolution, vec2(1.0));
    vec2 textureResolution = max(uTextureResolution, vec2(1.0));
    float streak = clamp(float(uStreakLength), 1.0, 512.0);
    vec2 logicalPixel = vTexCoord * logicalResolution;
    vec2 perpendicular = vec2(-direction.y, direction.x);
    float along = dot(logicalPixel, direction);
    float line = dot(logicalPixel, perpendicular);
    float segment = floor(along / max(streak * 0.65, 16.0));
    float noise = hash(vec2(floor(line), segment));
    float threshold = clamp(uThreshold, 0.0, 1.0);

    vec2 texel = 1.0 / textureResolution;
    vec2 resolutionScale = textureResolution / logicalResolution;
    float pixelScale = length(direction * resolutionScale);
    float streakPixels = max(streak * pixelScale, 1.0);
    float randomAmount = clamp(uRandomness, 0.0, 1.0);
    float localStreakPixels = streakPixels * mix(1.0, mix(0.55, 1.45, noise), randomAmount);
    float jitter = (noise - 0.5) * randomAmount * localStreakPixels * 0.18;
    vec3 accum = vec3(0.0);
    float total = 0.0;
    vec3 best = source.rgb;
    float bestMetric = metric;
    int sampleCount = int(clamp(ceil(localStreakPixels / 8.0), 12.0, 64.0));

    for (int i = 1; i <= 64; ++i) {
        if (i > sampleCount) {
            break;
        }
        float t = float(i - 1) / max(float(sampleCount - 1), 1.0);
        float distancePx = t * localStreakPixels + jitter;
        vec2 uv = clamp(vTexCoord - direction * texel * distancePx, vec2(0.0), vec2(1.0));
        vec3 sampleColor = texture(uSource, uv).rgb;
        float sampleMetric = sortMetric(sampleColor);
        float gate = smoothstep(threshold - 0.03, threshold + 0.15, sampleMetric);
        accum += sampleColor * gate;
        total += gate;
        if ((uReverse == 0 && sampleMetric > bestMetric) || (uReverse == 1 && sampleMetric < bestMetric)) {
            bestMetric = sampleMetric;
            best = sampleColor;
        }
    }

    vec3 smeared = total > 0.001 ? accum / total : source.rgb;
    vec3 sortedColor = mix(smeared, best, 0.45);
    float streakMask = smoothstep(0.05, 0.55, total / max(float(sampleCount), 1.0));
    fragColor = vec4(mix(source.rgb, sortedColor, clamp(uIntensity, 0.0, 1.0) * streakMask), source.a);
}
)";

constexpr const char* PostFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uSource;
uniform vec2 uResolution;
uniform float uTime;
uniform int uBloom;
uniform int uGrain;
uniform int uChromatic;
uniform int uScanlines;
uniform int uVignette;
uniform int uCrtCurve;
uniform int uPhosphor;
uniform float uBloomThreshold;
uniform float uBloomSoftThreshold;
uniform float uBloomIntensity;
uniform float uBloomRadius;
uniform float uGrainIntensity;
uniform float uGrainSize;
uniform float uGrainSpeed;
uniform int uGrainType;
uniform float uChromaticAmount;
uniform float uScanlineIntensity;
uniform float uVignetteIntensity;
uniform float uCrtCurveAmount;
uniform float uPhosphorStrength;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float random(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float randomSeeded(vec2 p, float seed) {
    return fract(sin(dot(p, vec2(12.9898, 78.233)) + seed * 37.719) * 43758.5453);
}

float smoothRandom(vec2 p, float seed) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = randomSeeded(i, seed);
    float b = randomSeeded(i + vec2(1.0, 0.0), seed);
    float c = randomSeeded(i + vec2(0.0, 1.0), seed);
    float d = randomSeeded(i + vec2(1.0, 1.0), seed);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float filmGrain(vec2 pixel, float frame) {
    float fine = randomSeeded(floor(pixel), frame * 11.0);
    float fine2 = randomSeeded(floor(pixel * 1.71), frame * 19.0);
    float soft = smoothRandom(pixel * 0.42, frame * 23.0);
    return ((fine + fine2 + soft) / 3.0) - 0.5;
}

float softGrain(vec2 pixel, float frame) {
    float soft = smoothRandom(pixel * 0.52, frame * 13.0);
    float fine = smoothRandom(pixel * 1.18, frame * 31.0);
    return mix(soft, fine, 0.35) - 0.5;
}

float coarseGrain(vec2 pixel, float frame) {
    float coarse = randomSeeded(floor(pixel * 0.55), frame * 17.0);
    float fine = randomSeeded(floor(pixel * 1.15), frame * 29.0);
    return mix(coarse, fine, 0.28) - 0.5;
}

float grainSample(vec2 pixel, float frame, int type) {
    if (type == 1) {
        return softGrain(pixel, frame);
    }
    if (type == 2) {
        return coarseGrain(pixel, frame);
    }
    return filmGrain(pixel, frame);
}

vec2 crtUv(vec2 uv) {
    vec2 centered = uv * 2.0 - 1.0;
    centered *= 1.0 + uCrtCurveAmount * dot(centered, centered);
    return centered * 0.5 + 0.5;
}

void main() {
    vec2 uv = vTexCoord;
    if (uCrtCurve == 1) {
        uv = crtUv(uv);
        if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    vec2 texel = 1.0 / max(uResolution, vec2(1.0));
    vec4 base = texture(uSource, uv);
    vec3 color = base.rgb;

    if (uChromatic == 1) {
        vec2 direction = normalize(uv - 0.5 + vec2(0.0001));
        vec2 offset = direction * uChromaticAmount * texel;
        color.r = texture(uSource, clamp(uv + offset, vec2(0.0), vec2(1.0))).r;
        color.b = texture(uSource, clamp(uv - offset, vec2(0.0), vec2(1.0))).b;
    }

    if (uBloom == 1) {
        vec3 bloom = vec3(0.0);
        float total = 0.0;
        float radius = max(uBloomRadius, 1.0);
        for (int y = -2; y <= 2; ++y) {
            for (int x = -2; x <= 2; ++x) {
                vec2 o = vec2(float(x), float(y)) * texel * radius;
                vec3 sampleColor = texture(uSource, clamp(uv + o, vec2(0.0), vec2(1.0))).rgb;
                float bright = smoothstep(uBloomThreshold, uBloomThreshold + max(uBloomSoftThreshold, 0.001), luma(sampleColor));
                bloom += sampleColor * bright;
                total += 1.0;
            }
        }
        color += (bloom / max(total, 1.0)) * uBloomIntensity;
    }

    if (uGrain == 1) {
        float grainScale = max(uGrainSize, 0.75);
        vec2 grainPixel = gl_FragCoord.xy / grainScale;
        float grainTime = uTime * max(uGrainSpeed, 1.0);
        float frame = floor(grainTime);
        float blend = smoothstep(0.0, 1.0, fract(grainTime));
        float grain = mix(grainSample(grainPixel, frame, uGrainType), grainSample(grainPixel, frame + 1.0, uGrainType), blend);
        float tone = luma(color);
        float highlightDamping = uGrainType == 2 ? 0.35 : 0.55;
        float tonalMask = smoothstep(0.02, 0.35, tone) * (1.0 - smoothstep(0.72, 1.0, tone) * highlightDamping);
        float typeAmount = uGrainType == 1 ? 0.34 : uGrainType == 2 ? 0.50 : 0.42;
        float chromaAmount = uGrainType == 3 ? 0.0 : uGrainType == 2 ? 0.10 : 0.18;
        float amount = (uGrainIntensity / 100.0) * typeAmount * tonalMask;
        vec3 chroma = vec3(
            grainSample(grainPixel + vec2(19.3, 7.1), frame, uGrainType),
            grainSample(grainPixel + vec2(5.7, 31.9), frame, uGrainType),
            grainSample(grainPixel + vec2(37.1, 17.4), frame, uGrainType)
        );
        color += vec3(grain) * amount;
        color += chroma * amount * chromaAmount;
    }

    if (uScanlines == 1) {
        float line = 0.5 + 0.5 * sin(uv.y * uResolution.y * 3.14159265);
        color *= 1.0 - (1.0 - line) * uScanlineIntensity;
    }

    if (uVignette == 1) {
        float d = distance(uv, vec2(0.5));
        float vig = smoothstep(0.35, 0.75, d);
        color *= 1.0 - vig * uVignetteIntensity;
    }

    if (uPhosphor == 1) {
        int stripe = int(mod(gl_FragCoord.x, 3.0));
        vec3 mask = stripe == 0 ? vec3(1.0, 1.0 - uPhosphorStrength, 1.0 - uPhosphorStrength)
                  : stripe == 1 ? vec3(1.0 - uPhosphorStrength, 1.0, 1.0 - uPhosphorStrength)
                                : vec3(1.0 - uPhosphorStrength, 1.0 - uPhosphorStrength, 1.0);
        color *= mask;
    }

    fragColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), base.a);
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

    preprocessShader_.compile(PassthroughVertexShader, PreprocessFragmentShader);
    passthroughShader_.compile(PassthroughVertexShader, PassthroughFragmentShader);
    asciiShader_.compile(PassthroughVertexShader, AsciiFragmentShader);
    ditherShader_.compile(PassthroughVertexShader, DitherFragmentShader);
    halftoneShader_.compile(PassthroughVertexShader, HalftoneFragmentShader);
    dotsShader_.compile(PassthroughVertexShader, DotsFragmentShader);
    contourShader_.compile(PassthroughVertexShader, ContourFragmentShader);
    pixelSortShader_.compile(PassthroughVertexShader, PixelSortFragmentShader);
    postShader_.compile(PassthroughVertexShader, PostFragmentShader);
    quad_.initialize();
    initialized_ = true;
}

GLuint PreviewPipeline::render(
    GLuint sourceTexture,
    int renderWidth,
    int renderHeight,
    int logicalWidth,
    int logicalHeight,
    const PreviewRenderSettings& settings
) {
    initialize();
    const int outputWidth = std::max(1, renderWidth);
    const int outputHeight = std::max(1, renderHeight);
    const int logicalRenderWidth = std::max(1, logicalWidth);
    const int logicalRenderHeight = std::max(1, logicalHeight);
    const GLuint effectSourceTexture = [&]() {
        if (settings.sourceAlreadyProcessed) {
            return sourceTexture;
        }

        preprocessTexture_.resize(outputWidth, outputHeight);
        preprocessFramebuffer_.attach(preprocessTexture_);
        preprocessFramebuffer_.bind();
        glViewport(0, 0, outputWidth, outputHeight);
        glDisable(GL_BLEND);
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);

        preprocessShader_.use();
        glActiveTexturePtr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        preprocessShader_.setInt("uSource", 0);
        preprocessShader_.setVec2("uResolution", static_cast<float>(logicalRenderWidth), static_cast<float>(logicalRenderHeight));
        preprocessShader_.setInt("uInvert", settings.context.processing.invert ? 1 : 0);
        preprocessShader_.setFloat("uBrightness", settings.context.adjustments.brightness);
        preprocessShader_.setFloat("uContrast", settings.context.adjustments.contrast);
        preprocessShader_.setFloat("uSaturation", settings.context.adjustments.saturation);
        preprocessShader_.setFloat("uHueRotation", settings.context.adjustments.hueRotationDegrees);
        preprocessShader_.setFloat("uSharpness", settings.context.adjustments.sharpness);
        preprocessShader_.setFloat("uGamma", settings.context.adjustments.gamma);
        preprocessShader_.setFloat("uBrightnessMap", settings.context.processing.brightnessMap);
        preprocessShader_.setFloat("uEdgeEnhance", settings.context.processing.edgeEnhance);
        preprocessShader_.setFloat("uBlur", settings.context.processing.blur);
        preprocessShader_.setFloat("uQuantizeColors", static_cast<float>(settings.context.processing.quantizeColors));
        preprocessShader_.setFloat("uShapeMatching", settings.context.processing.shapeMatching);
        quad_.draw();
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgramPtr(0);
        Framebuffer::bindDefault();
        return preprocessTexture_.id();
    }();

    effectTexture_.resize(outputWidth, outputHeight);
    effectFramebuffer_.attach(effectTexture_);
    effectFramebuffer_.bind();
    glViewport(0, 0, outputWidth, outputHeight);
    glDisable(GL_BLEND);
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    ShaderProgram* effectShader = &passthroughShader_;
    if (settings.effect == PreviewEffect::Ascii) {
        effectShader = &asciiShader_;
    } else if (settings.effect == PreviewEffect::Dither) {
        effectShader = &ditherShader_;
    } else if (settings.effect == PreviewEffect::Halftone) {
        effectShader = &halftoneShader_;
    } else if (settings.effect == PreviewEffect::Dots) {
        effectShader = &dotsShader_;
    } else if (settings.effect == PreviewEffect::Contour) {
        effectShader = &contourShader_;
    } else if (settings.effect == PreviewEffect::PixelSort) {
        effectShader = &pixelSortShader_;
    }

    effectShader->use();
    glActiveTexturePtr(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectSourceTexture);
    effectShader->setInt("uSource", 0);
    effectShader->setVec2("uResolution", static_cast<float>(logicalRenderWidth), static_cast<float>(logicalRenderHeight));

    if (settings.effect == PreviewEffect::Ascii) {
        glActiveTexturePtr(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, settings.ascii.glyphAtlasTexture);
        glActiveTexturePtr(GL_TEXTURE0);
        effectShader->setInt("uGlyphAtlas", 1);
        effectShader->setFloat("uScale", settings.ascii.scale);
        effectShader->setFloat("uSpacing", settings.ascii.spacing);
        effectShader->setInt("uOutputWidth", settings.ascii.outputWidth);
        effectShader->setInt("uCharacterSet", settings.ascii.characterSet);
        effectShader->setInt("uAtlasColumns", settings.ascii.atlasColumns);
        effectShader->setInt("uAtlasRows", settings.ascii.atlasRows);
    } else if (settings.effect == PreviewEffect::Dither) {
        effectShader->setInt("uAlgorithm", settings.dither.algorithm);
        effectShader->setFloat("uIntensity", settings.dither.intensity);
        effectShader->setInt("uModulation", settings.dither.modulation ? 1 : 0);
    } else if (settings.effect == PreviewEffect::Halftone) {
        effectShader->setFloat("uDotScale", settings.halftone.dotScale);
        effectShader->setFloat("uSpacing", settings.halftone.spacing);
        effectShader->setFloat("uAngle", settings.halftone.angleDegrees * 0.017453292519943295F);
        effectShader->setInt("uShape", settings.halftone.shape);
        effectShader->setInt("uInvert", settings.halftone.invert ? 1 : 0);
    } else if (settings.effect == PreviewEffect::Dots) {
        effectShader->setFloat("uSize", settings.dots.size);
        effectShader->setFloat("uSpacing", settings.dots.spacing);
        effectShader->setInt("uShape", settings.dots.shape);
        effectShader->setInt("uGridType", settings.dots.gridType);
        effectShader->setInt("uInvert", settings.dots.invert ? 1 : 0);
    } else if (settings.effect == PreviewEffect::Contour) {
        effectShader->setFloat("uLevels", settings.contour.levels);
        effectShader->setFloat("uLineThickness", settings.contour.lineThickness);
        effectShader->setInt("uInvert", settings.contour.invert ? 1 : 0);
    } else if (settings.effect == PreviewEffect::PixelSort) {
        effectShader->setInt("uDirection", settings.pixelSort.direction);
        effectShader->setInt("uSortMode", settings.pixelSort.sortMode);
        effectShader->setFloat("uThreshold", settings.pixelSort.threshold);
        effectShader->setInt("uStreakLength", settings.pixelSort.streakLength);
        effectShader->setFloat("uIntensity", settings.pixelSort.intensity);
        effectShader->setFloat("uRandomness", settings.pixelSort.randomness);
        effectShader->setInt("uReverse", settings.pixelSort.reverse ? 1 : 0);
        effectShader->setVec2("uTextureResolution", static_cast<float>(outputWidth), static_cast<float>(outputHeight));
    }

    quad_.draw();
    if (settings.effect == PreviewEffect::Ascii) {
        glActiveTexturePtr(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexturePtr(GL_TEXTURE0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgramPtr(0);
    Framebuffer::bindDefault();

    outputTexture_.resize(outputWidth, outputHeight);
    outputFramebuffer_.attach(outputTexture_);
    outputFramebuffer_.bind();
    glViewport(0, 0, outputWidth, outputHeight);
    glDisable(GL_BLEND);
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    postShader_.use();
    glActiveTexturePtr(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectTexture_.id());
    postShader_.setInt("uSource", 0);
    postShader_.setVec2("uResolution", static_cast<float>(logicalRenderWidth), static_cast<float>(logicalRenderHeight));
    postShader_.setFloat("uTime", settings.timeSeconds);
    postShader_.setInt("uBloom", settings.bloom ? 1 : 0);
    postShader_.setInt("uGrain", settings.grain ? 1 : 0);
    postShader_.setInt("uChromatic", settings.chromatic ? 1 : 0);
    postShader_.setInt("uScanlines", settings.scanlines ? 1 : 0);
    postShader_.setInt("uVignette", settings.vignette ? 1 : 0);
    postShader_.setInt("uCrtCurve", settings.crtCurve ? 1 : 0);
    postShader_.setInt("uPhosphor", settings.phosphor ? 1 : 0);
    postShader_.setFloat("uBloomThreshold", settings.bloomThreshold);
    postShader_.setFloat("uBloomSoftThreshold", settings.bloomSoftThreshold);
    postShader_.setFloat("uBloomIntensity", settings.bloomIntensity);
    postShader_.setFloat("uBloomRadius", settings.bloomRadius);
    postShader_.setFloat("uGrainIntensity", settings.grainIntensity);
    postShader_.setFloat("uGrainSize", settings.grainSize);
    postShader_.setFloat("uGrainSpeed", settings.grainSpeed);
    postShader_.setInt("uGrainType", settings.grainType);
    postShader_.setFloat("uChromaticAmount", settings.chromaticAmount);
    postShader_.setFloat("uScanlineIntensity", settings.scanlineIntensity);
    postShader_.setFloat("uVignetteIntensity", settings.vignetteIntensity);
    postShader_.setFloat("uCrtCurveAmount", settings.crtCurveAmount);
    postShader_.setFloat("uPhosphorStrength", settings.phosphorStrength);
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
    preprocessFramebuffer_.reset();
    preprocessTexture_.reset();
    effectFramebuffer_.reset();
    effectTexture_.reset();
    outputFramebuffer_.reset();
    outputTexture_.reset();
    quad_.reset();
    preprocessShader_ = ShaderProgram();
    passthroughShader_ = ShaderProgram();
    asciiShader_ = ShaderProgram();
    ditherShader_ = ShaderProgram();
    halftoneShader_ = ShaderProgram();
    dotsShader_ = ShaderProgram();
    contourShader_ = ShaderProgram();
    pixelSortShader_ = ShaderProgram();
    postShader_ = ShaderProgram();
    initialized_ = false;
}

} // namespace ShaderLoom::app
