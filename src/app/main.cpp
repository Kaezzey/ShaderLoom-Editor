#include "ShaderLoom/Image.hpp"
#include "ShaderLoom/effects/AsciiEffect.hpp"
#include "ShaderLoom/effects/DitherEffect.hpp"
#include "ShaderLoom/effects/PixelSortEffect.hpp"
#include "app/render/GLPipeline.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace {

constexpr float LeftRailWidth = 232.0F;
constexpr float RightRailWidth = 300.0F;
constexpr float FooterHeight = 28.0F;
constexpr int GlyphAtlasColumns = 16;
constexpr int GlyphAtlasRows = 7;
constexpr int GlyphAtlasAsciiCount = 95;
constexpr int GlyphAtlasBlockStart = GlyphAtlasAsciiCount;

ImFont* gLogoFont = nullptr;

struct LoadedImageState {
    ShaderLoom::Image image;
    std::vector<ShaderLoom::ImageFrame> frames;
    std::filesystem::path path;
    GLuint texture = 0;
    float zoom = 1.0F;
    ImVec2 pan = ImVec2(0.0F, 0.0F);
    std::string error;
    int frameIndex = 0;
    double frameAccumulator = 0.0;
    double lastFrameTime = 0.0;

    [[nodiscard]] bool hasImage() const noexcept {
        return texture != 0 && !image.empty();
    }

    [[nodiscard]] bool isAnimated() const noexcept {
        return frames.size() > 1;
    }
};

struct RenderState {
    ShaderLoom::app::PreviewPipeline previewPipeline;
    GLuint previewTexture = 0;
    GLuint cpuEffectTexture = 0;
    GLuint glyphAtlasTexture = 0;
    GLuint nebulaTexture = 0;
    std::string cpuEffectCacheKey;
    std::string error;
    std::string exportStatus;
    bool deferCpuEffectUpdate = false;
};

enum class ExportFormat {
    Png = 0,
    Jpeg,
    Gif,
    Video,
    Svg,
    Text,
    ThreeJs
};

struct AppSettings {
    ShaderLoom::app::PreviewRenderSettings preview;
    ShaderLoom::RenderContext context;
    ShaderLoom::AsciiSettings ascii;
    int asciiCharacterSet = 0;
    ShaderLoom::DitherSettings dither;
    ShaderLoom::PixelSortSettings pixelSort;
    ExportFormat exportFormat = ExportFormat::Png;
    bool processingOpen = true;
    bool postOpen = true;
    bool exportOpen = true;
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
    float grainIntensity = 18.0F;
    float grainSize = 1.0F;
    float grainSpeed = 48.0F;
    int grainType = 0;
    float chromaticAmount = 6.0F;
    float scanlineIntensity = 0.25F;
    float vignetteIntensity = 0.45F;
    float crtCurveAmount = 0.12F;
    float phosphorStrength = 0.35F;
};

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

std::string truncateMiddle(const std::string& value, std::size_t maxLength) {
    if (value.size() <= maxLength) {
        return value;
    }
    if (maxLength < 8) {
        return value.substr(0, maxLength);
    }

    const std::size_t left = (maxLength - 3) / 2;
    const std::size_t right = maxLength - 3 - left;
    return value.substr(0, left) + "..." + value.substr(value.size() - right);
}

void destroyTexture(LoadedImageState& state) {
    if (state.texture != 0) {
        glDeleteTextures(1, &state.texture);
        state.texture = 0;
    }
}

void destroyRenderTextures(RenderState& state) {
    if (state.cpuEffectTexture != 0) {
        glDeleteTextures(1, &state.cpuEffectTexture);
        state.cpuEffectTexture = 0;
    }
    if (state.glyphAtlasTexture != 0) {
        glDeleteTextures(1, &state.glyphAtlasTexture);
        state.glyphAtlasTexture = 0;
    }
    if (state.nebulaTexture != 0) {
        glDeleteTextures(1, &state.nebulaTexture);
        state.nebulaTexture = 0;
    }
    state.cpuEffectCacheKey.clear();
}

void uploadImageToTexture(GLuint& texture, const ShaderLoom::Image& image) {
    if (texture == 0) {
        glGenTextures(1, &texture);
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        image.width(),
        image.height(),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.pixels().data()
    );
    glBindTexture(GL_TEXTURE_2D, 0);
}

void updateTexturePixels(GLuint texture, const ShaderLoom::Image& image) {
    if (texture == 0 || image.empty()) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        image.width(),
        image.height(),
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.pixels().data()
    );
    glBindTexture(GL_TEXTURE_2D, 0);
}

float hashNoise(int x, int y) {
    std::uint32_t hash = static_cast<std::uint32_t>(x) * 374761393U + static_cast<std::uint32_t>(y) * 668265263U;
    hash = (hash ^ (hash >> 13U)) * 1274126177U;
    hash ^= hash >> 16U;
    return static_cast<float>(hash & 0x00FFFFFFU) / 16777215.0F;
}

float smoothNoise(float x, float y) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float sx = tx * tx * (3.0F - 2.0F * tx);
    const float sy = ty * ty * (3.0F - 2.0F * ty);
    const float n00 = hashNoise(x0, y0);
    const float n10 = hashNoise(x0 + 1, y0);
    const float n01 = hashNoise(x0, y0 + 1);
    const float n11 = hashNoise(x0 + 1, y0 + 1);
    const float nx0 = n00 + (n10 - n00) * sx;
    const float nx1 = n01 + (n11 - n01) * sx;
    return nx0 + (nx1 - nx0) * sy;
}

float fbm(float x, float y) {
    float value = 0.0F;
    float amplitude = 0.5F;
    float frequency = 1.0F;
    for (int i = 0; i < 6; ++i) {
        value += smoothNoise(x * frequency, y * frequency) * amplitude;
        frequency *= 2.03F;
        amplitude *= 0.52F;
    }
    return value;
}

float gaussian2(float x, float y, float cx, float cy, float sx, float sy) {
    const float dx = (x - cx) / sx;
    const float dy = (y - cy) / sy;
    return std::exp(-((dx * dx) + (dy * dy)));
}

std::uint8_t colorByte(float value) {
    return static_cast<std::uint8_t>(std::clamp(std::round(value * 255.0F), 0.0F, 255.0F));
}

GLuint createNebulaTexture() {
    constexpr int width = 1536;
    constexpr int height = 1024;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4), 255);

    for (int y = 0; y < height; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(height - 1);
        for (int x = 0; x < width; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(width - 1);
            const float px = (u * 2.0F - 1.0F) * 1.5F;
            const float py = v * 2.0F - 1.0F;
            const float warpX = fbm((u * 2.8F) + 18.0F, (v * 2.1F) + 4.0F) - 0.5F;
            const float warpY = fbm((u * 1.9F) - 8.0F, (v * 2.7F) + 23.0F) - 0.5F;
            const float wx = px + warpX * 0.42F;
            const float wy = py + warpY * 0.34F;
            const float largeNoise = fbm((wx * 1.2F) + 6.0F, (wy * 1.2F) - 3.0F);
            const float fineNoise = fbm((wx * 5.3F) + 41.0F, (wy * 4.7F) + 9.0F);
            const float purpleField = gaussian2(wx, wy, -0.72F, 0.74F, 1.00F, 0.64F) + gaussian2(wx, wy, -0.16F, 0.45F, 0.92F, 0.72F) * 0.38F;
            const float blueField = gaussian2(wx, wy, 0.62F, 0.52F, 0.92F, 0.72F) + gaussian2(wx, wy, 0.18F, -0.16F, 1.18F, 0.82F) * 0.46F;
            const float highMist = gaussian2(wx, wy, 0.02F, -0.72F, 1.65F, 0.72F);
            const float filament = std::pow(std::clamp(fineNoise * 1.28F + largeNoise * 0.36F - 0.58F, 0.0F, 1.0F), 2.1F);
            const float dust = std::pow(std::clamp(largeNoise * 1.18F - 0.34F, 0.0F, 1.0F), 1.55F);
            const float vignette = std::clamp(1.15F - std::sqrt((px * px * 0.35F) + (py * py * 0.72F)), 0.0F, 1.0F);

            float r = 0.002F;
            float g = 0.003F;
            float b = 0.012F;
            r += purpleField * (0.055F + dust * 0.030F) + filament * 0.040F;
            g += blueField * (0.040F + dust * 0.026F) + highMist * 0.008F;
            b += blueField * (0.150F + dust * 0.080F) + purpleField * 0.090F + highMist * 0.055F + filament * 0.060F;
            r *= 0.46F + vignette * 0.78F;
            g *= 0.42F + vignette * 0.80F;
            b *= 0.46F + vignette * 0.86F;

            const float dither = (hashNoise(x * 3 + 17, y * 5 - 11) - 0.5F) / 120.0F;
            const auto index = static_cast<std::size_t>((y * width + x) * 4);
            pixels[index] = colorByte(std::clamp(r + dither, 0.0F, 1.0F));
            pixels[index + 1] = colorByte(std::clamp(g + dither, 0.0F, 1.0F));
            pixels[index + 2] = colorByte(std::clamp(b + dither, 0.0F, 1.0F));
            pixels[index + 3] = 255;
        }
    }

    ShaderLoom::Image nebula(width, height, std::move(pixels));
    GLuint texture = 0;
    uploadImageToTexture(texture, nebula);
    return texture;
}

#ifdef _WIN32
GLuint createAsciiGlyphAtlas(int tileWidth, int tileHeight, int columns, int rows) {
    const int atlasWidth = tileWidth * columns;
    const int atlasHeight = tileHeight * rows;
    std::vector<wchar_t> glyphs;
    glyphs.reserve(static_cast<std::size_t>(GlyphAtlasAsciiCount + 4));
    for (int code = 32; code <= 126; ++code) {
        glyphs.push_back(static_cast<wchar_t>(code));
    }
    glyphs.push_back(0x2591);
    glyphs.push_back(0x2592);
    glyphs.push_back(0x2593);
    glyphs.push_back(0x2588);

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = atlasWidth;
    info.bmiHeader.biHeight = -atlasHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (dc == nullptr || bitmap == nullptr || bits == nullptr) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        if (dc != nullptr) {
            DeleteDC(dc);
        }
        return 0;
    }

    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    RECT fullRect{0, 0, atlasWidth, atlasHeight};
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &fullRect, blackBrush);
    DeleteObject(blackBrush);

    HFONT font = CreateFontW(
        -static_cast<int>(static_cast<float>(tileHeight) * 0.72F),
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN,
        L"Consolas"
    );
    HGDIOBJ oldFont = font != nullptr ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));

    for (int tile = 0; tile < static_cast<int>(glyphs.size()); ++tile) {
        const int x = (tile % columns) * tileWidth;
        const int y = (tile / columns) * tileHeight;
        RECT rect{x, y, x + tileWidth, y + tileHeight};
        const wchar_t text[2] = {glyphs[static_cast<std::size_t>(tile)], L'\0'};
        DrawTextW(dc, text, 1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX);
    }

    std::vector<std::uint8_t> coverage(static_cast<std::size_t>(atlasWidth * atlasHeight), 0);
    const auto* bgra = static_cast<const std::uint8_t*>(bits);
    for (int y = 0; y < atlasHeight; ++y) {
        for (int x = 0; x < atlasWidth; ++x) {
            const auto source = static_cast<std::size_t>((y * atlasWidth + x) * 4);
            coverage[static_cast<std::size_t>(y * atlasWidth + x)] = std::max({bgra[source], bgra[source + 1], bgra[source + 2]});
        }
    }

    if (oldFont != nullptr) {
        SelectObject(dc, oldFont);
    }
    SelectObject(dc, oldBitmap);
    if (font != nullptr) {
        DeleteObject(font);
    }
    DeleteObject(bitmap);
    DeleteDC(dc);

    const int spread = std::clamp(tileHeight / 6, 8, 14);
    const int spreadSquared = spread * spread;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(atlasWidth * atlasHeight * 4), 0);

    constexpr std::uint8_t sdfInsideThreshold = 1;

    for (int y = 0; y < atlasHeight; ++y) {
        const int tileY = y / tileHeight;
        const int tileMinY = tileY * tileHeight;
        const int tileMaxY = std::min(tileMinY + tileHeight, atlasHeight);
        for (int x = 0; x < atlasWidth; ++x) {
            const int tileX = x / tileWidth;
            const int tileMinX = tileX * tileWidth;
            const int tileMaxX = std::min(tileMinX + tileWidth, atlasWidth);
            const auto pixelIndex = static_cast<std::size_t>(y * atlasWidth + x);
            const bool inside = coverage[pixelIndex] >= sdfInsideThreshold;
            int nearestSquared = spreadSquared;

            for (int dy = -spread; dy <= spread; ++dy) {
                const int sampleY = y + dy;
                if (sampleY < tileMinY || sampleY >= tileMaxY) {
                    continue;
                }
                const int dySquared = dy * dy;
                if (dySquared >= nearestSquared) {
                    continue;
                }
                for (int dx = -spread; dx <= spread; ++dx) {
                    const int sampleX = x + dx;
                    if (sampleX < tileMinX || sampleX >= tileMaxX) {
                        continue;
                    }
                    const int distanceSquared = dx * dx + dySquared;
                    if (distanceSquared >= nearestSquared || distanceSquared > spreadSquared) {
                        continue;
                    }
                    const bool sampleInside = coverage[static_cast<std::size_t>(sampleY * atlasWidth + sampleX)] >= sdfInsideThreshold;
                    if (sampleInside != inside) {
                        nearestSquared = distanceSquared;
                    }
                }
            }

            const float nearest = std::sqrt(static_cast<float>(nearestSquared));
            const float signedDistance = inside ? nearest : -nearest;
            const float distanceValue = std::clamp(0.5F + (signedDistance / static_cast<float>(spread * 2)), 0.0F, 1.0F);
            const auto rgbaIndex = static_cast<std::size_t>((y * atlasWidth + x) * 4);
            rgba[rgbaIndex] = 255;
            rgba[rgbaIndex + 1] = 255;
            rgba[rgbaIndex + 2] = 255;
            rgba[rgbaIndex + 3] = static_cast<std::uint8_t>(std::round(distanceValue * 255.0F));
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}
#else
GLuint createAsciiGlyphAtlas(int, int, int, int) {
    return 0;
}
#endif

bool loadImage(LoadedImageState& state, const std::filesystem::path& path) {
    try {
        std::vector<ShaderLoom::ImageFrame> frames = ShaderLoom::loadImageFrames(path);
        ShaderLoom::Image loaded = frames.front().image;

        GLuint texture = 0;
        uploadImageToTexture(texture, loaded);

        destroyTexture(state);
        state.image = std::move(loaded);
        state.frames = std::move(frames);
        state.path = path;
        state.texture = texture;
        state.zoom = 1.0F;
        state.pan = ImVec2(0.0F, 0.0F);
        state.frameIndex = 0;
        state.frameAccumulator = 0.0;
        state.lastFrameTime = glfwGetTime();
        state.error.clear();
        return true;
    } catch (const std::exception& error) {
        state.error = error.what();
        return false;
    }
}

void clearImage(LoadedImageState& state) {
    destroyTexture(state);
    state.image = ShaderLoom::Image();
    state.frames.clear();
    state.path.clear();
    state.zoom = 1.0F;
    state.pan = ImVec2(0.0F, 0.0F);
    state.frameIndex = 0;
    state.frameAccumulator = 0.0;
    state.lastFrameTime = 0.0;
    state.error.clear();
}

void advanceAnimation(LoadedImageState& state, double nowSeconds) {
    if (!state.isAnimated() || state.texture == 0) {
        state.lastFrameTime = nowSeconds;
        return;
    }

    if (state.lastFrameTime <= 0.0) {
        state.lastFrameTime = nowSeconds;
        return;
    }

    state.frameAccumulator += std::max(0.0, nowSeconds - state.lastFrameTime) * 1000.0;
    state.lastFrameTime = nowSeconds;

    bool advanced = false;
    while (state.frameAccumulator >= static_cast<double>(state.frames[static_cast<std::size_t>(state.frameIndex)].durationMs)) {
        state.frameAccumulator -= static_cast<double>(state.frames[static_cast<std::size_t>(state.frameIndex)].durationMs);
        state.frameIndex = (state.frameIndex + 1) % static_cast<int>(state.frames.size());
        advanced = true;
    }

    if (advanced) {
        updateTexturePixels(state.texture, state.frames[static_cast<std::size_t>(state.frameIndex)].image);
    }
}

ShaderLoom::app::PreviewEffect effectForIndex(int selectedEffect) {
    if (selectedEffect == 0) {
        return ShaderLoom::app::PreviewEffect::Ascii;
    }
    if (selectedEffect == 1) {
        return ShaderLoom::app::PreviewEffect::Dither;
    }
    if (selectedEffect == 2) {
        return ShaderLoom::app::PreviewEffect::Halftone;
    }
    if (selectedEffect == 3) {
        return ShaderLoom::app::PreviewEffect::Dots;
    }
    if (selectedEffect == 4) {
        return ShaderLoom::app::PreviewEffect::Contour;
    }
    if (selectedEffect == 5) {
        return ShaderLoom::app::PreviewEffect::PixelSort;
    }
    return ShaderLoom::app::PreviewEffect::Passthrough;
}

bool isCpuEffect(int selectedEffect) {
    return selectedEffect == 5;
}

int maxRenderTextureSize() {
    static const int textureSize = [] {
        GLint value = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
        return std::max(value, 1024);
    }();
    return textureSize;
}

const char* asciiSetName(int index) {
    static constexpr const char* Names[] = {
        "STANDARD",
        "BLOCKS",
        "BINARY",
        "DETAILED",
        "MINIMAL",
        "ALPHABETIC",
        "NUMERIC",
        "MATH",
        "SYMBOLS"
    };
    return Names[std::clamp(index, 0, static_cast<int>(std::size(Names)) - 1)];
}

using GlyphRows = std::array<std::uint8_t, 7>;

GlyphRows densityGlyph(std::uint32_t glyph) {
    static const std::string Ramp = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
    const std::size_t position = glyph <= 0x7FU ? Ramp.find(static_cast<char>(glyph)) : std::string::npos;
    const float density = position == std::string::npos
        ? 0.55F
        : static_cast<float>(position) / static_cast<float>(Ramp.size() - 1);

    if (density < 0.08F) {
        return {0, 0, 0, 0, 0, 0, 0};
    }
    if (density < 0.20F) {
        return {0, 0, 0, 4, 0, 0, 0};
    }
    if (density < 0.34F) {
        return {0, 4, 0, 4, 0, 4, 0};
    }
    if (density < 0.48F) {
        return {0, 4, 14, 4, 14, 4, 0};
    }
    if (density < 0.62F) {
        return {17, 10, 4, 10, 17, 0, 0};
    }
    if (density < 0.76F) {
        return {21, 10, 21, 10, 21, 10, 21};
    }
    if (density < 0.90F) {
        return {31, 17, 31, 17, 31, 17, 31};
    }
    return {31, 31, 31, 31, 31, 31, 31};
}

GlyphRows glyphRows(std::uint32_t glyph) {
    if (glyph == 0x2591U) { return {17, 4, 17, 4, 17, 4, 17}; }
    if (glyph == 0x2592U) { return {21, 10, 21, 10, 21, 10, 21}; }
    if (glyph == 0x2593U) { return {29, 23, 29, 23, 29, 23, 29}; }
    if (glyph == 0x2588U) { return {31, 31, 31, 31, 31, 31, 31}; }

    const char asciiGlyph = glyph <= 0x7FU ? static_cast<char>(glyph) : '?';
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(asciiGlyph)));
    switch (c) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 14};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {14, 4, 4, 4, 4, 4, 14};
    case 'J': return {1, 1, 1, 1, 17, 17, 14};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {6, 8, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 2, 12};
    default:
        break;
    }

    switch (asciiGlyph) {
    case ' ': return {0, 0, 0, 0, 0, 0, 0};
    case '.': return {0, 0, 0, 0, 0, 12, 12};
    case ',': return {0, 0, 0, 0, 0, 12, 8};
    case ':': return {0, 12, 12, 0, 12, 12, 0};
    case ';': return {0, 12, 12, 0, 12, 8, 0};
    case '\'': return {4, 4, 8, 0, 0, 0, 0};
    case '`': return {8, 4, 0, 0, 0, 0, 0};
    case '"': return {10, 10, 0, 0, 0, 0, 0};
    case '^': return {4, 10, 17, 0, 0, 0, 0};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case '_': return {0, 0, 0, 0, 0, 0, 31};
    case '+': return {0, 4, 4, 31, 4, 4, 0};
    case '=': return {0, 0, 31, 0, 31, 0, 0};
    case '*': return {0, 21, 14, 31, 14, 21, 0};
    case '#': return {10, 31, 10, 10, 31, 10, 0};
    case '%': return {24, 25, 2, 4, 8, 19, 3};
    case '@': return {14, 17, 23, 21, 23, 16, 14};
    case '$': return {4, 15, 20, 14, 5, 30, 4};
    case '&': return {12, 18, 20, 8, 21, 18, 13};
    case '?': return {14, 17, 1, 2, 4, 0, 4};
    case '!': return {4, 4, 4, 4, 4, 0, 4};
    case '<': return {2, 4, 8, 16, 8, 4, 2};
    case '>': return {8, 4, 2, 1, 2, 4, 8};
    case '~': return {0, 0, 8, 21, 2, 0, 0};
    case '|': return {4, 4, 4, 4, 4, 4, 4};
    case '/': return {1, 2, 2, 4, 8, 8, 16};
    case '\\': return {16, 8, 8, 4, 2, 2, 1};
    case '[': return {14, 8, 8, 8, 8, 8, 14};
    case ']': return {14, 2, 2, 2, 2, 2, 14};
    case '{': return {2, 4, 4, 8, 4, 4, 2};
    case '}': return {8, 4, 4, 2, 4, 4, 8};
    case '(': return {2, 4, 8, 8, 8, 4, 2};
    case ')': return {8, 4, 2, 2, 2, 4, 8};
    default:
        return densityGlyph(glyph);
    }
}

bool glyphBit(const GlyphRows& rows, int x, int y) {
    return (rows[static_cast<std::size_t>(y)] & static_cast<std::uint8_t>(1 << (4 - x))) != 0;
}

ShaderLoom::Image renderAsciiRaster(
    const ShaderLoom::Image& source,
    const ShaderLoom::AsciiSettings& settings,
    const ShaderLoom::RenderContext& context
) {
    ShaderLoom::AsciiEffect ascii;
    const ShaderLoom::AsciiResult result = ascii.generate(source, settings, context);
    ShaderLoom::Image output(source.width(), source.height());
    std::vector<std::uint8_t>& pixels = output.pixels();
    std::fill(pixels.begin(), pixels.end(), 0);
    for (std::size_t i = 3; i < pixels.size(); i += 4) {
        pixels[i] = 255;
    }

    const int columns = std::max(1, result.columns);
    const int rows = std::max(1, result.rows);
    for (int row = 0; row < rows; ++row) {
        const int y0 = (row * source.height()) / rows;
        const int y1 = std::max(y0 + 1, ((row + 1) * source.height()) / rows);
        for (int col = 0; col < columns; ++col) {
            const int x0 = (col * source.width()) / columns;
            const int x1 = std::max(x0 + 1, ((col + 1) * source.width()) / columns);
            const int sampleX = std::clamp((x0 + x1) / 2, 0, source.width() - 1);
            const int sampleY = std::clamp((y0 + y1) / 2, 0, source.height() - 1);
            const ShaderLoom::Pixel color = ShaderLoom::applyProcessing(source.pixel(sampleX, sampleY), context);
            const std::uint32_t glyph = result.glyphCodes[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            const GlyphRows rowsForGlyph = glyphRows(glyph);
            const int cellWidth = std::max(1, x1 - x0);
            const int cellHeight = std::max(1, y1 - y0);
            const int padX = cellWidth > 7 ? 1 : 0;
            const int padY = cellHeight > 9 ? 1 : 0;
            const int drawWidth = std::max(1, cellWidth - (padX * 2));
            const int drawHeight = std::max(1, cellHeight - (padY * 2));

            for (int y = y0; y < y1; ++y) {
                if (y < y0 + padY || y >= y1 - padY) {
                    continue;
                }
                const int localY = std::clamp(y - y0 - padY, 0, drawHeight - 1);
                const int glyphY = std::clamp((localY * 7) / drawHeight, 0, 6);
                for (int x = x0; x < x1; ++x) {
                    if (x < x0 + padX || x >= x1 - padX) {
                        continue;
                    }
                    const int localX = std::clamp(x - x0 - padX, 0, drawWidth - 1);
                    const int glyphX = std::clamp((localX * 5) / drawWidth, 0, 4);
                    if (!glyphBit(rowsForGlyph, glyphX, glyphY)) {
                        continue;
                    }
                    const auto index = static_cast<std::size_t>((y * source.width() + x) * 4);
                    pixels[index] = color.r;
                    pixels[index + 1] = color.g;
                    pixels[index + 2] = color.b;
                    pixels[index + 3] = color.a;
                }
            }
        }
    }

    return output;
}

void syncPreviewSettings(AppSettings& settings, int selectedEffect, float timeSeconds, GLuint glyphAtlasTexture) {
    settings.ascii.characterSet = asciiSetName(settings.asciiCharacterSet);
    settings.preview.effect = isCpuEffect(selectedEffect)
        ? ShaderLoom::app::PreviewEffect::Passthrough
        : effectForIndex(selectedEffect);
    settings.preview.context = settings.context;
    settings.preview.sourceAlreadyProcessed = isCpuEffect(selectedEffect);
    settings.preview.renderScale = std::clamp(std::round(settings.preview.renderScale), 1.0F, 4.0F);
    settings.preview.ascii.scale = settings.ascii.scale;
    settings.preview.ascii.spacing = settings.ascii.spacing;
    settings.preview.ascii.outputWidth = settings.ascii.outputWidth;
    settings.preview.ascii.characterSet = settings.asciiCharacterSet;
    settings.preview.ascii.glyphAtlasTexture = glyphAtlasTexture;
    settings.preview.ascii.atlasColumns = GlyphAtlasColumns;
    settings.preview.ascii.atlasRows = GlyphAtlasRows;
    settings.preview.dither.algorithm = static_cast<int>(settings.dither.algorithm);
    settings.preview.dither.intensity = settings.dither.intensity;
    settings.preview.dither.modulation = settings.dither.modulation;
    settings.preview.pixelSort.direction = static_cast<int>(settings.pixelSort.direction);
    settings.preview.pixelSort.sortMode = static_cast<int>(settings.pixelSort.sortMode);
    settings.preview.pixelSort.threshold = settings.pixelSort.threshold;
    settings.preview.pixelSort.streakLength = settings.pixelSort.streakLength;
    settings.preview.pixelSort.intensity = settings.pixelSort.intensity;
    settings.preview.pixelSort.randomness = settings.pixelSort.randomness;
    settings.preview.pixelSort.reverse = settings.pixelSort.reverse;
    settings.preview.bloom = settings.bloom;
    settings.preview.grain = settings.grain;
    settings.preview.chromatic = settings.chromatic;
    settings.preview.scanlines = settings.scanlines;
    settings.preview.vignette = settings.vignette;
    settings.preview.crtCurve = settings.crtCurve;
    settings.preview.phosphor = settings.phosphor;
    settings.preview.bloomThreshold = settings.bloomThreshold;
    settings.preview.bloomSoftThreshold = settings.bloomSoftThreshold;
    settings.preview.bloomIntensity = settings.bloomIntensity;
    settings.preview.bloomRadius = settings.bloomRadius;
    settings.preview.grainIntensity = settings.grainIntensity;
    settings.preview.grainSize = settings.grainSize;
    settings.preview.grainSpeed = settings.grainSpeed;
    settings.preview.grainType = settings.grainType;
    settings.preview.chromaticAmount = settings.chromaticAmount;
    settings.preview.scanlineIntensity = settings.scanlineIntensity;
    settings.preview.vignetteIntensity = settings.vignetteIntensity;
    settings.preview.crtCurveAmount = settings.crtCurveAmount;
    settings.preview.phosphorStrength = settings.phosphorStrength;
    settings.preview.timeSeconds = timeSeconds;
}

std::string cpuEffectCacheKey(const LoadedImageState& imageState, int selectedEffect, const AppSettings& settings) {
    std::ostringstream key;
    key << selectedEffect << '|'
        << imageState.path.string() << '|'
        << imageState.image.width() << 'x' << imageState.image.height() << '|'
        << settings.context.adjustments.brightness << '|'
        << settings.context.adjustments.contrast << '|'
        << settings.context.adjustments.saturation << '|'
        << settings.context.adjustments.hueRotationDegrees << '|'
        << settings.context.adjustments.sharpness << '|'
        << settings.context.adjustments.gamma << '|'
        << settings.context.processing.invert << '|'
        << settings.context.processing.brightnessMap << '|'
        << settings.context.processing.edgeEnhance << '|'
        << settings.context.processing.blur << '|'
        << settings.context.processing.quantizeColors << '|'
        << settings.context.processing.shapeMatching << '|';

    if (selectedEffect == 0) {
        key << settings.ascii.scale << '|'
            << settings.ascii.spacing << '|'
            << settings.ascii.outputWidth << '|'
            << settings.ascii.characterSet << '|';
    } else if (selectedEffect == 1) {
        key << static_cast<int>(settings.dither.algorithm) << '|'
            << settings.dither.intensity << '|'
            << settings.dither.modulation;
    } else if (selectedEffect == 5) {
        key << static_cast<int>(settings.pixelSort.direction) << '|'
            << static_cast<int>(settings.pixelSort.sortMode) << '|'
            << settings.pixelSort.threshold << '|'
            << settings.pixelSort.streakLength << '|'
            << settings.pixelSort.intensity << '|'
            << settings.pixelSort.randomness << '|'
            << settings.pixelSort.reverse;
    }
    return key.str();
}

GLuint sourceTextureForRender(LoadedImageState& imageState, RenderState& renderState, int selectedEffect, const AppSettings& settings) {
    if (!imageState.hasImage() || !isCpuEffect(selectedEffect)) {
        return imageState.texture;
    }

    const std::string key = cpuEffectCacheKey(imageState, selectedEffect, settings);
    if (renderState.cpuEffectTexture != 0 && renderState.cpuEffectCacheKey == key) {
        return renderState.cpuEffectTexture;
    }
    if (renderState.deferCpuEffectUpdate && renderState.cpuEffectTexture != 0) {
        return renderState.cpuEffectTexture;
    }

    ShaderLoom::Image processed = imageState.image;
    if (selectedEffect == 0) {
        processed = renderAsciiRaster(imageState.image, settings.ascii, settings.context);
    } else if (selectedEffect == 1) {
        ShaderLoom::DitherEffect dither;
        processed = dither.apply(imageState.image, settings.dither, settings.context);
    } else if (selectedEffect == 5) {
        ShaderLoom::PixelSortEffect pixelSort;
        processed = pixelSort.apply(imageState.image, settings.pixelSort, settings.context);
    }

    uploadImageToTexture(renderState.cpuEffectTexture, processed);
    renderState.cpuEffectCacheKey = key;
    return renderState.cpuEffectTexture;
}

void renderPreviewPipeline(LoadedImageState& imageState, RenderState& renderState, int selectedEffect, const AppSettings& settings) {
    renderState.previewTexture = 0;
    renderState.error.clear();

    if (!imageState.hasImage()) {
        return;
    }

    try {
        const GLuint sourceTexture = sourceTextureForRender(imageState, renderState, selectedEffect, settings);
        const float requestedScale = std::clamp(settings.preview.renderScale, 1.0F, 4.0F);
        const int textureLimit = maxRenderTextureSize();
        const float maxScaleX = static_cast<float>(textureLimit) / static_cast<float>(std::max(imageState.image.width(), 1));
        const float maxScaleY = static_cast<float>(textureLimit) / static_cast<float>(std::max(imageState.image.height(), 1));
        const float effectiveScale = std::max(1.0F, std::min({requestedScale, maxScaleX, maxScaleY}));
        const int renderWidth = std::max(1, static_cast<int>(std::round(static_cast<float>(imageState.image.width()) * effectiveScale)));
        const int renderHeight = std::max(1, static_cast<int>(std::round(static_cast<float>(imageState.image.height()) * effectiveScale)));
        renderState.previewTexture = renderState.previewPipeline.render(
            sourceTexture,
            renderWidth,
            renderHeight,
            imageState.image.width(),
            imageState.image.height(),
            settings.preview
        );
    } catch (const std::exception& error) {
        renderState.error = error.what();
        renderState.previewTexture = imageState.texture;
    }
}

const char* exportFormatName(ExportFormat format) {
    switch (format) {
    case ExportFormat::Png:
        return "PNG";
    case ExportFormat::Jpeg:
        return "JPEG";
    case ExportFormat::Gif:
        return "GIF";
    case ExportFormat::Video:
        return "Video";
    case ExportFormat::Svg:
        return "SVG";
    case ExportFormat::Text:
        return "Text";
    case ExportFormat::ThreeJs:
        return "Three.js";
    }
    return "PNG";
}

const char* exportExtension(ExportFormat format) {
    switch (format) {
    case ExportFormat::Jpeg:
        return ".jpg";
    case ExportFormat::Gif:
        return ".gif";
    case ExportFormat::Video:
        return ".mp4";
    case ExportFormat::Svg:
        return ".svg";
    case ExportFormat::Text:
        return ".txt";
    case ExportFormat::ThreeJs:
        return ".html";
    case ExportFormat::Png:
    default:
        return ".png";
    }
}

bool canExportRaster(ExportFormat format) {
    return format == ExportFormat::Png || format == ExportFormat::Jpeg;
}

#ifdef _WIN32
std::optional<std::filesystem::path> browseForImage() {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg;*.gif\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0GIF\0*.gif\0All Files\0*.*\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = "png";

    if (GetOpenFileNameA(&dialog) == TRUE) {
        return std::filesystem::path(filename);
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> browseForExportPath(ExportFormat format, const std::filesystem::path& sourcePath) {
    char filename[MAX_PATH] = {};
    const std::string stem = sourcePath.empty() ? "ShaderLoom_export" : sourcePath.stem().string() + "_ShaderLoom";
    const std::string defaultName = stem + exportExtension(format);
    strncpy_s(filename, defaultName.c_str(), MAX_PATH - 1);

    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFilter = "PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0All Files\0*.*\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = format == ExportFormat::Jpeg ? "jpg" : "png";

    if (GetSaveFileNameA(&dialog) == TRUE) {
        std::filesystem::path selected(filename);
        if (selected.extension().empty()) {
            selected += exportExtension(format);
        }
        return selected;
    }
    return std::nullopt;
}
#else
std::optional<std::filesystem::path> browseForImage() {
    return std::nullopt;
}

std::optional<std::filesystem::path> browseForExportPath(ExportFormat, const std::filesystem::path&) {
    return std::nullopt;
}
#endif

void dropCallback(GLFWwindow* window, int count, const char** paths) {
    auto* state = static_cast<LoadedImageState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr || count <= 0 || paths == nullptr) {
        return;
    }
    loadImage(*state, std::filesystem::path(paths[0]));
}

void loadShaderLoomFonts() {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* uiFont = nullptr;
#ifdef _WIN32
    uiFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 14.0F);
    gLogoFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\bahnschrift.ttf", 30.0F);
#endif
    if (uiFont == nullptr) {
        uiFont = io.Fonts->AddFontDefault();
    }
    io.FontDefault = uiFont;
    if (gLogoFont == nullptr) {
        gLogoFont = uiFont;
    }
}

void applyShaderLoomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0F;
    style.ChildRounding = 0.0F;
    style.FrameRounding = 0.0F;
    style.PopupRounding = 0.0F;
    style.ScrollbarRounding = 0.0F;
    style.GrabRounding = 0.0F;
    style.WindowBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;
    style.ItemSpacing = ImVec2(8.0F, 8.0F);
    style.FramePadding = ImVec2(6.0F, 4.0F);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.86F, 0.91F, 1.0F, 1.0F);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.38F, 0.46F, 0.58F, 1.0F);
    colors[ImGuiCol_WindowBg] = ImVec4(0.005F, 0.012F, 0.030F, 0.96F);
    colors[ImGuiCol_ChildBg] = ImVec4(0.010F, 0.020F, 0.045F, 0.78F);
    colors[ImGuiCol_PopupBg] = ImVec4(0.006F, 0.012F, 0.030F, 0.96F);
    colors[ImGuiCol_Border] = ImVec4(0.20F, 0.34F, 0.58F, 0.46F);
    colors[ImGuiCol_FrameBg] = ImVec4(0.010F, 0.018F, 0.040F, 0.92F);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.030F, 0.070F, 0.145F, 0.96F);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.050F, 0.105F, 0.210F, 1.0F);
    colors[ImGuiCol_Button] = ImVec4(0.020F, 0.045F, 0.090F, 0.90F);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.070F, 0.150F, 0.280F, 0.95F);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.120F, 0.220F, 0.420F, 1.0F);
    colors[ImGuiCol_CheckMark] = ImVec4(0.80F, 0.90F, 1.0F, 1.0F);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.52F, 0.70F, 1.0F, 1.0F);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.92F, 0.96F, 1.0F, 1.0F);
    colors[ImGuiCol_Header] = ImVec4(0.025F, 0.055F, 0.110F, 0.86F);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.060F, 0.120F, 0.230F, 0.95F);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.100F, 0.190F, 0.360F, 1.0F);
    colors[ImGuiCol_Separator] = ImVec4(0.16F, 0.28F, 0.46F, 0.48F);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.34F, 0.52F, 0.88F, 0.78F);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.56F, 0.72F, 1.0F, 1.0F);
}

void drawPs2Backdrop(ImDrawList* drawList, const ImVec2& min, const ImVec2& size, float time, float intensity, GLuint texture) {
    (void)time;
    const ImVec2 max(min.x + size.x, min.y + size.y);
    if (texture != 0) {
        drawList->AddImage(static_cast<ImTextureID>(texture), min, max);
    } else {
        drawList->AddRectFilledMultiColor(
            min,
            max,
            IM_COL32(0, 0, 5, 255),
            IM_COL32(3, 2, 18, 255),
            IM_COL32(5, 12, 36, 255),
            IM_COL32(18, 5, 32, 255)
        );
    }

    (void)intensity;
}

void drawPanelAtmosphere(ImDrawList* drawList, const ImVec2& min, const ImVec2& size) {
    const ImVec2 max(min.x + size.x, min.y + size.y);
    drawList->AddRectFilledMultiColor(
        min,
        max,
        IM_COL32(4, 12, 30, 196),
        IM_COL32(2, 8, 22, 210),
        IM_COL32(0, 3, 11, 232),
        IM_COL32(1, 7, 19, 220)
    );
    drawList->AddRect(min, max, IM_COL32(52, 88, 150, 92));
    drawList->AddLine(ImVec2(max.x - 1.0F, min.y), ImVec2(max.x - 1.0F, max.y), IM_COL32(112, 154, 226, 44), 1.0F);
}

void drawShaderLoomWordmark(ImDrawList* drawList, const ImVec2& pos, float size) {
    const char* text = "ShaderLoom";
    ImFont* font = gLogoFont != nullptr ? gLogoFont : ImGui::GetFont();
    drawList->AddText(font, size, ImVec2(pos.x - 2.0F, pos.y + 2.0F), IM_COL32(0, 0, 0, 180), text);
    drawList->AddText(font, size, ImVec2(pos.x - 1.0F, pos.y), IM_COL32(72, 124, 255, 46), text);
    drawList->AddText(font, size, ImVec2(pos.x + 1.0F, pos.y), IM_COL32(72, 124, 255, 46), text);
    drawList->AddText(font, size, pos, IM_COL32(232, 242, 255, 245), text);
    const ImVec2 extent = font->CalcTextSizeA(size, 10000.0F, 0.0F, text);
    drawList->AddLine(
        ImVec2(pos.x + 2.0F, pos.y + extent.y + 2.0F),
        ImVec2(pos.x + extent.x - 2.0F, pos.y + extent.y + 2.0F),
        IM_COL32(84, 120, 255, 125),
        1.0F
    );
}

void sectionTitle(const char* title) {
    ImGui::Spacing();
    ImGui::TextUnformatted(title);
    ImGui::Separator();
}

bool sectionToggle(const char* title, bool& open) {
    ImGui::Spacing();
    const std::string label = std::string(open ? "- " : "+ ") + title;
    if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0.0F, 22.0F))) {
        open = !open;
    }
    ImGui::Separator();
    return open;
}

void valueSlider(const char* label, float* value, float min, float max, const char* format = "%.1f") {
    ImGui::PushID(label);
    std::string visibleLabel = label;
    const std::size_t idSeparator = visibleLabel.find("##");
    if (idSeparator != std::string::npos) {
        visibleLabel.resize(idSeparator);
    }
    ImGui::TextDisabled("%s", visibleLabel.c_str());
    ImGui::SameLine(92.0F);
    ImGui::Text(format, *value);
    ImGui::SameLine(138.0F);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::SliderFloat("##slider", value, min, max, "");
    ImGui::PopID();
}

bool formatTile(const char* name, const char* extension, bool selected, const ImVec2& size) {
    ImGui::PushID(name);
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.08F, 0.08F, 0.08F, 1.0F) : ImVec4(0.045F, 0.047F, 0.047F, 1.0F));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10F, 0.11F, 0.11F, 1.0F));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12F, 0.13F, 0.13F, 1.0F));
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.80F, 0.80F, 0.80F, 1.0F));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.12F, 0.13F, 0.13F, 1.0F));
    }

    const bool clicked = ImGui::Button("##format-tile", size);
    const ImVec2 min = ImGui::GetItemRectMin();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(min.x + 10.0F, min.y + 10.0F), IM_COL32(205, 216, 230, 255), name);
    drawList->AddText(ImVec2(min.x + 10.0F, min.y + 28.0F), IM_COL32(92, 96, 100, 255), extension);

    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return clicked;
}

void exportRenderedImage(const LoadedImageState& imageState, RenderState& renderState, ExportFormat format) {
    if (!imageState.hasImage()) {
        renderState.exportStatus = "Load an image first.";
        return;
    }
    if (!canExportRaster(format)) {
        renderState.exportStatus = std::string(exportFormatName(format)) + " export is not wired yet.";
        return;
    }
    if (!renderState.previewPipeline.hasOutput()) {
        renderState.exportStatus = "No rendered frame is ready yet.";
        return;
    }

    try {
        const std::optional<std::filesystem::path> selectedPath = browseForExportPath(format, imageState.path);
        if (!selectedPath) {
            return;
        }

        ShaderLoom::Image output = renderState.previewPipeline.readOutputImage();
        if (format == ExportFormat::Jpeg) {
            output.writeJpeg(*selectedPath);
        } else {
            output.writePng(*selectedPath);
        }
        renderState.exportStatus = "Exported " + selectedPath->filename().string();
    } catch (const std::exception& error) {
        renderState.exportStatus = error.what();
    }
}

void drawLeftRail(int& selectedEffect, LoadedImageState& imageState) {
    ImGui::BeginChild("left-rail", ImVec2(LeftRailWidth, 0.0F), true);
    drawPanelAtmosphere(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowSize());
    ImGui::SetCursorPos(ImVec2(14.0F, 13.0F));
    drawShaderLoomWordmark(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), 23.0F);
    ImGui::Dummy(ImVec2(1.0F, 34.0F));
    ImGui::Separator();

    sectionTitle("- Input");
    ImGui::TextDisabled("%s", imageState.hasImage() ? "Image loaded" : "No image");
    if (imageState.hasImage()) {
        ImGui::SameLine(174.0F);
        if (ImGui::SmallButton("Clear")) {
            clearImage(imageState);
        }
    }
    ImGui::TextDisabled("Resolution");
    ImGui::SameLine(124.0F);
    if (imageState.hasImage()) {
        ImGui::TextDisabled("%d x %d", imageState.image.width(), imageState.image.height());
    } else {
        ImGui::TextDisabled("-");
    }
    ImGui::TextDisabled("File");
    ImGui::SameLine(124.0F);
    if (imageState.hasImage()) {
        ImGui::TextDisabled("%s", truncateMiddle(imageState.path.filename().string(), 18).c_str());
    } else {
        ImGui::TextDisabled("-");
    }

    if (ImGui::Button("Drop file or click to browse\nPNG, JPG, GIF", ImVec2(-1.0F, 56.0F))) {
        if (const std::optional<std::filesystem::path> selected = browseForImage()) {
            loadImage(imageState, *selected);
        }
    }

    if (!imageState.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95F, 0.45F, 0.45F, 1.0F));
        ImGui::TextWrapped("%s", imageState.error.c_str());
        ImGui::PopStyleColor();
    }

    sectionTitle("- Effects");
    const char* effects[] = {
        "ASCII",
        "Dithering",
        "Halftone",
        "Dots",
        "Contour",
        "Pixel Sort"
    };

    for (int i = 0; i < static_cast<int>(sizeof(effects) / sizeof(effects[0])); ++i) {
        const bool active = selectedEffect == i;
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.82F, 0.90F, 1.0F, 1.0F) : ImVec4(0.35F, 0.37F, 0.38F, 1.0F));
        const std::string label = std::string(active ? "*  " : "o  ") + effects[i];
        if (ImGui::Selectable(label.c_str(), active, 0, ImVec2(0.0F, 18.0F))) {
            selectedEffect = i;
        }
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - FooterHeight - 42.0F);
    sectionTitle("+ Presets");
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - FooterHeight);
    ImGui::TextDisabled("Follow   About   Changelog");
    ImGui::EndChild();
}

void drawPreview(const char* effectName, LoadedImageState& imageState, RenderState& renderState, float width, float timeSeconds) {
    ImGui::BeginChild("preview", ImVec2(width, 0.0F), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 previewMin = ImGui::GetWindowPos();
    const ImVec2 previewSize = ImGui::GetWindowSize();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawPs2Backdrop(drawList, previewMin, previewSize, timeSeconds, 1.0F, renderState.nebulaTexture);

    const std::string title = std::string(effectName) + (imageState.hasImage() ? " [OPENGL]" : " [READY]");
    const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());
    ImGui::SetCursorPos(ImVec2((previewSize.x - titleSize.x) * 0.5F, 16.0F));
    ImGui::TextDisabled("%s", title.c_str());

    ImGui::SetCursorPos(ImVec2(previewSize.x - 86.0F, 14.0F));
    ImGui::SmallButton("[]");
    ImGui::SameLine();
    ImGui::SmallButton("<>");
    ImGui::SameLine();
    ImGui::SmallButton("::");

    const ImVec2 contentMin(previewMin.x + 18.0F, previewMin.y + 54.0F);
    const ImVec2 contentMax(previewMin.x + previewSize.x - 18.0F, previewMin.y + previewSize.y - FooterHeight - 12.0F);
    drawList->PushClipRect(contentMin, contentMax, true);

    if (imageState.hasImage()) {
        const float availableWidth = std::max(1.0F, contentMax.x - contentMin.x);
        const float availableHeight = std::max(1.0F, contentMax.y - contentMin.y);
        const float fit = std::min(
            availableWidth / static_cast<float>(imageState.image.width()),
            availableHeight / static_cast<float>(imageState.image.height())
        );

        if (ImGui::IsWindowHovered()) {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && io.MouseWheel != 0.0F) {
                imageState.zoom = std::clamp(imageState.zoom + (io.MouseWheel * 0.08F), 0.1F, 8.0F);
            }
            if (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                imageState.pan.x += io.MouseDelta.x;
                imageState.pan.y += io.MouseDelta.y;
            }
        }

        const float scale = fit * imageState.zoom;
        const float imageWidth = static_cast<float>(imageState.image.width()) * scale;
        const float imageHeight = static_cast<float>(imageState.image.height()) * scale;
        const ImVec2 imageMin(
            contentMin.x + ((availableWidth - imageWidth) * 0.5F) + imageState.pan.x,
            contentMin.y + ((availableHeight - imageHeight) * 0.5F) + imageState.pan.y
        );
        const ImVec2 imageMax(imageMin.x + imageWidth, imageMin.y + imageHeight);
        const GLuint previewTexture = renderState.previewTexture != 0 ? renderState.previewTexture : imageState.texture;

        drawList->AddRectFilled(ImVec2(imageMin.x - 6.0F, imageMin.y - 6.0F), ImVec2(imageMax.x + 6.0F, imageMax.y + 6.0F), IM_COL32(2, 7, 18, 180));
        drawList->AddRect(ImVec2(imageMin.x - 6.0F, imageMin.y - 6.0F), ImVec2(imageMax.x + 6.0F, imageMax.y + 6.0F), IM_COL32(74, 118, 205, 100));
        drawList->AddRectFilled(imageMin, imageMax, IM_COL32(2, 4, 9, 255));
        drawList->AddImage(
            static_cast<ImTextureID>(previewTexture),
            imageMin,
            imageMax,
            ImVec2(0.0F, 0.0F),
            ImVec2(1.0F, 1.0F)
        );
        drawList->AddRect(imageMin, imageMax, IM_COL32(34, 36, 36, 255));

        if (!renderState.error.empty()) {
            drawList->AddText(
                ImVec2(imageMin.x + 10.0F, imageMin.y + 10.0F),
                IM_COL32(242, 116, 116, 255),
                renderState.error.c_str()
            );
        }
    } else {
        const ImVec2 emptyMin(contentMin.x + 36.0F, contentMin.y + 72.0F);
        const ImVec2 emptyMax(contentMax.x - 36.0F, contentMax.y - 72.0F);
        drawList->AddRect(emptyMin, emptyMax, IM_COL32(46, 58, 102, 42));
        const char* emptyText = "Drop a PNG, JPG, or GIF here";
        const ImVec2 emptyTextSize = ImGui::CalcTextSize(emptyText);
        drawList->AddText(
            ImVec2((emptyMin.x + emptyMax.x - emptyTextSize.x) * 0.5F, (emptyMin.y + emptyMax.y - emptyTextSize.y) * 0.5F),
            IM_COL32(90, 104, 142, 210),
            emptyText
        );
    }
    drawList->PopClipRect();

    if (imageState.hasImage()) {
        ImGui::SetCursorPos(ImVec2(previewSize.x - 220.0F, previewSize.y - FooterHeight));
        if (ImGui::SmallButton("-")) {
            imageState.zoom = std::max(0.1F, imageState.zoom - 0.1F);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d%%", static_cast<int>(std::round(imageState.zoom * 100.0F)));
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) {
            imageState.zoom = std::min(8.0F, imageState.zoom + 0.1F);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) {
            imageState.zoom = 1.0F;
            imageState.pan = ImVec2(0.0F, 0.0F);
        }
    }

    ImGui::SetCursorPos(ImVec2(18.0F, previewSize.y - FooterHeight));
    ImGui::TextDisabled("Scroll to pan  Ctrl+Scroll to zoom  Alt+Drag to pan");
    ImGui::EndChild();
}

void drawExportSection(const LoadedImageState& imageState, RenderState& renderState, AppSettings& settings) {
    if (!sectionToggle("Export", settings.exportOpen)) {
        return;
    }
    ImGui::TextDisabled("Format");

    const char* names[] = {"PNG", "JPEG", "GIF", "Video", "SVG", "Text", "Three.js"};
    const char* extensions[] = {".png", ".jpg", ".gif", ".mp4", ".svg", ".txt", ".html"};
    int selectedFormat = static_cast<int>(settings.exportFormat);

    const float gap = 6.0F;
    const float tileWidth = (ImGui::GetContentRegionAvail().x - gap) * 0.5F;
    const ImVec2 tileSize(tileWidth, 52.0F);

    for (int i = 0; i < 7; ++i) {
        if (formatTile(names[i], extensions[i], selectedFormat == i, tileSize)) {
            selectedFormat = i;
            settings.exportFormat = static_cast<ExportFormat>(i);
        }
        if ((i % 2) == 0 && i != 6) {
            ImGui::SameLine(0.0F, gap);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled(canExportRaster(settings.exportFormat) ? "High quality image" : "Planned export format");

    const std::string buttonLabel = std::string("Export ") + exportFormatName(settings.exportFormat);
    if (!canExportRaster(settings.exportFormat)) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1.0F, 30.0F))) {
        exportRenderedImage(imageState, renderState, settings.exportFormat);
    }
    if (!canExportRaster(settings.exportFormat)) {
        ImGui::EndDisabled();
    }

    if (!renderState.exportStatus.empty()) {
        ImGui::TextWrapped("%s", renderState.exportStatus.c_str());
    }
}

void drawSettingsRail(const char* effectName, int selectedEffect, const LoadedImageState& imageState, RenderState& renderState, AppSettings& settings) {
    ImGui::BeginChild("settings-rail", ImVec2(RightRailWidth, 0.0F), true);
    drawPanelAtmosphere(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowSize());
    ImGui::TextUnformatted("- Settings");
    ImGui::SameLine(RightRailWidth - 56.0F);
    ImGui::TextDisabled("Reset");

    sectionTitle(effectName);
    ImGui::PushID("effect-settings");
    if (selectedEffect == 0) {
        valueSlider("Scale", &settings.ascii.scale, 0.1F, 4.0F, "%.1f");
        valueSlider("Spacing", &settings.ascii.spacing, 0.0F, 2.0F);
        float outputWidth = static_cast<float>(settings.ascii.outputWidth);
        valueSlider("Output Width", &outputWidth, 0.0F, 4096.0F, "%.0f");
        settings.ascii.outputWidth = static_cast<int>(std::round(outputWidth));
        const char* characterSets[] = {"STANDARD", "BLOCKS", "BINARY", "DETAILED", "MINIMAL", "ALPHABETIC", "NUMERIC", "MATH", "SYMBOLS"};
        ImGui::TextDisabled("Character Set");
        ImGui::SameLine(92.0F);
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::Combo("##ascii-character-set", &settings.asciiCharacterSet, characterSets, static_cast<int>(std::size(characterSets)))) {
            settings.ascii.characterSet = asciiSetName(settings.asciiCharacterSet);
        }
    } else if (selectedEffect == 1) {
        const char* algorithms[] = {
            "Floyd-Steinberg",
            "Atkinson",
            "Jarvis-Judice-Ninke",
            "Stucki",
            "Burkes",
            "Sierra",
            "Sierra Two-Row",
            "Sierra Lite",
            "Bayer 2x2"
        };
        int algorithm = static_cast<int>(settings.dither.algorithm);
        ImGui::TextDisabled("Algorithm");
        ImGui::SameLine(92.0F);
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::Combo("##dither-algorithm", &algorithm, algorithms, static_cast<int>(std::size(algorithms)))) {
            settings.dither.algorithm = static_cast<ShaderLoom::DitherAlgorithm>(algorithm);
        }
        valueSlider("Intensity", &settings.dither.intensity, 0.0F, 1.0F);
        ImGui::Checkbox("Modulation", &settings.dither.modulation);
    } else if (selectedEffect == 2) {
        const char* shapes[] = {"Circle", "Square", "Diamond", "Line"};
        ImGui::TextDisabled("Shape");
        ImGui::SameLine(92.0F);
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::Combo("##halftone-shape", &settings.preview.halftone.shape, shapes, static_cast<int>(std::size(shapes)));
        valueSlider("Dot Scale", &settings.preview.halftone.dotScale, 0.05F, 1.5F);
        valueSlider("Spacing", &settings.preview.halftone.spacing, 2.0F, 48.0F);
        valueSlider("Angle", &settings.preview.halftone.angleDegrees, -90.0F, 90.0F, "%.0f deg");
        ImGui::Checkbox("Invert", &settings.preview.halftone.invert);
    } else if (selectedEffect == 3) {
        const char* shapes[] = {"Circle", "Square", "Diamond"};
        ImGui::TextDisabled("Shape");
        ImGui::SameLine(92.0F);
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::Combo("##dots-shape", &settings.preview.dots.shape, shapes, static_cast<int>(std::size(shapes)));
        const char* grids[] = {"Square Grid", "Hexagonal Grid"};
        ImGui::TextDisabled("Grid Type");
        ImGui::SameLine(92.0F);
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::Combo("##dots-grid", &settings.preview.dots.gridType, grids, static_cast<int>(std::size(grids)));
        valueSlider("Size", &settings.preview.dots.size, 0.05F, 2.0F);
        valueSlider("Spacing", &settings.preview.dots.spacing, 4.0F, 64.0F);
        ImGui::Checkbox("Invert", &settings.preview.dots.invert);
    } else if (selectedEffect == 4) {
        const char* fillModes[] = {"Filled Bands"};
        static int fillMode = 0;
        ImGui::TextDisabled("Fill Mode");
        ImGui::SameLine(92.0F);
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::Combo("##contour-fill", &fillMode, fillModes, 1);
        valueSlider("Levels", &settings.preview.contour.levels, 2.0F, 32.0F, "%.0f");
        valueSlider("Line Thickness", &settings.preview.contour.lineThickness, 0.5F, 6.0F);
        ImGui::Checkbox("Invert", &settings.preview.contour.invert);
    } else if (selectedEffect == 5) {
        const char* directions[] = {"Horizontal", "Vertical", "Diagonal"};
        int direction = static_cast<int>(settings.pixelSort.direction);
        ImGui::TextDisabled("Direction");
        ImGui::SameLine(92.0F);
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::Combo("##pixelsort-direction", &direction, directions, static_cast<int>(std::size(directions)))) {
            settings.pixelSort.direction = static_cast<ShaderLoom::PixelSortDirection>(direction);
        }
        const char* sortModes[] = {"Brightness", "Hue", "Saturation"};
        int sortMode = static_cast<int>(settings.pixelSort.sortMode);
        ImGui::TextDisabled("Sort Mode");
        ImGui::SameLine(92.0F);
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::Combo("##pixelsort-mode", &sortMode, sortModes, static_cast<int>(std::size(sortModes)))) {
            settings.pixelSort.sortMode = static_cast<ShaderLoom::PixelSortMode>(sortMode);
        }
        valueSlider("Threshold", &settings.pixelSort.threshold, 0.0F, 1.0F);
        float streakLength = static_cast<float>(settings.pixelSort.streakLength);
        valueSlider("Streak Length", &streakLength, 1.0F, 512.0F, "%.0f");
        settings.pixelSort.streakLength = static_cast<int>(std::round(streakLength));
        valueSlider("Intensity", &settings.pixelSort.intensity, 0.0F, 1.0F);
        valueSlider("Randomness", &settings.pixelSort.randomness, 0.0F, 1.0F);
        ImGui::Checkbox("Reverse", &settings.pixelSort.reverse);
    } else {
        ImGui::TextDisabled("Preview pass-through");
    }
    ImGui::PopID();

    ImGui::PushID("adjustments");
    sectionTitle("Adjustments");
    valueSlider("Brightness", &settings.context.adjustments.brightness, -100.0F, 100.0F, "%.0f");
    valueSlider("Contrast", &settings.context.adjustments.contrast, -100.0F, 100.0F, "%.0f");
    if (selectedEffect == 0) {
        valueSlider("Saturation", &settings.context.adjustments.saturation, -100.0F, 100.0F, "%.0f");
        valueSlider("Hue Rotation", &settings.context.adjustments.hueRotationDegrees, -180.0F, 180.0F, "%.0f deg");
    }
    valueSlider(selectedEffect == 1 ? "Sharpen" : "Sharpness", &settings.context.adjustments.sharpness, 0.0F, 5.0F, "%.1f");
    valueSlider("Gamma", &settings.context.adjustments.gamma, 0.1F, 4.0F);
    ImGui::PopID();

    ImGui::PushID("color");
    sectionTitle("Color");
    const char* modes[] = {"Original", "Monochrome", "Duotone"};
    static int mode = 0;
    ImGui::TextDisabled("Mode");
    ImGui::SameLine(92.0F);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::Combo("##mode", &mode, modes, 3);
    static char background[] = "#000000";
    ImGui::TextDisabled("Background");
    ImGui::SameLine(92.0F);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##background", background, sizeof(background));
    static float intensity = 1.1F;
    valueSlider("Intensity", &intensity, 0.0F, 2.0F);
    ImGui::PopID();

    if (sectionToggle("Processing", settings.processingOpen)) {
        ImGui::PushID("processing");
        valueSlider("Render Scale", &settings.preview.renderScale, 1.0F, 4.0F, "%.0fx");
        settings.preview.renderScale = std::clamp(std::round(settings.preview.renderScale), 1.0F, 4.0F);
        ImGui::Checkbox("Invert", &settings.context.processing.invert);
        valueSlider("Brightness Map", &settings.context.processing.brightnessMap, 0.1F, 4.0F);
        valueSlider("Edge Enhance", &settings.context.processing.edgeEnhance, 0.0F, 5.0F, "%.0f");
        valueSlider("Blur", &settings.context.processing.blur, 0.0F, 10.0F);
        float quantize = static_cast<float>(settings.context.processing.quantizeColors);
        valueSlider("Quantize Colors", &quantize, 0.0F, 32.0F, "%.0f");
        settings.context.processing.quantizeColors = static_cast<int>(std::round(quantize));
        valueSlider("Shape Matching", &settings.context.processing.shapeMatching, 0.0F, 1.0F);
        ImGui::PopID();
    }

    if (sectionToggle("Post-Processing", settings.postOpen)) {
        ImGui::PushID("post-processing");
        ImGui::Checkbox("Bloom", &settings.bloom);
        if (settings.bloom) {
            valueSlider("Threshold##bloom", &settings.bloomThreshold, 0.0F, 1.0F);
            valueSlider("Soft Threshold", &settings.bloomSoftThreshold, 0.0F, 2.0F);
            valueSlider("Intensity##bloom", &settings.bloomIntensity, 0.0F, 2.0F);
            valueSlider("Radius", &settings.bloomRadius, 0.0F, 32.0F, "%.0f");
        }
        ImGui::Checkbox("Grain", &settings.grain);
        if (settings.grain) {
            const char* grainTypes[] = {"Fine", "Soft", "Coarse", "Monochrome"};
            ImGui::TextDisabled("Type");
            ImGui::SameLine(92.0F);
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::Combo("##grain-type", &settings.grainType, grainTypes, static_cast<int>(std::size(grainTypes)));
            valueSlider("Intensity##grain", &settings.grainIntensity, 0.0F, 100.0F, "%.0f");
            valueSlider("Size", &settings.grainSize, 0.0F, 2.0F, "%.1f");
            valueSlider("Speed", &settings.grainSpeed, 1.0F, 120.0F, "%.0f fps");
        }
        ImGui::Checkbox("Chromatic", &settings.chromatic);
        if (settings.chromatic) {
            valueSlider("Amount##chromatic", &settings.chromaticAmount, 0.0F, 32.0F, "%.0f");
        }
        ImGui::Checkbox("Scanlines", &settings.scanlines);
        if (settings.scanlines) {
            valueSlider("Intensity##scanlines", &settings.scanlineIntensity, 0.0F, 1.0F);
        }
        ImGui::Checkbox("Vignette", &settings.vignette);
        if (settings.vignette) {
            valueSlider("Intensity##vignette", &settings.vignetteIntensity, 0.0F, 1.0F);
        }
        ImGui::Checkbox("CRT Curve", &settings.crtCurve);
        if (settings.crtCurve) {
            valueSlider("Amount##crt", &settings.crtCurveAmount, 0.0F, 0.5F);
        }
        ImGui::Checkbox("Phosphor", &settings.phosphor);
        if (settings.phosphor) {
            valueSlider("Strength##phosphor", &settings.phosphorStrength, 0.0F, 1.0F);
        }
        ImGui::PopID();
    }

    drawExportSection(imageState, renderState, settings);
    ImGui::EndChild();
}

} // namespace

int main(int argc, char** argv) {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1440, 900, "ShaderLoom Offline Editor", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    try {
        ShaderLoom::app::loadOpenGLPipelineFunctions();
    } catch (const std::exception& error) {
        std::cerr << "OpenGL setup failed: " << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    LoadedImageState imageState;
    RenderState renderState;
    AppSettings appSettings;
    glfwSetWindowUserPointer(window, &imageState);
    glfwSetDropCallback(window, dropCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    loadShaderLoomFonts();
    applyShaderLoomStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    renderState.glyphAtlasTexture = createAsciiGlyphAtlas(48, 72, GlyphAtlasColumns, GlyphAtlasRows);
    renderState.nebulaTexture = createNebulaTexture();

    static int selectedEffect = 0;
    const char* effects[] = {
        "ASCII",
        "Dithering",
        "Halftone",
        "Dots",
        "Contour",
        "Pixel Sort"
    };

    if (argc > 1) {
        loadImage(imageState, std::filesystem::path(argv[1]));
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const double now = glfwGetTime();
        advanceAnimation(imageState, now);
        syncPreviewSettings(appSettings, selectedEffect, static_cast<float>(now), renderState.glyphAtlasTexture);
        renderPreviewPipeline(imageState, renderState, selectedEffect, appSettings);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("ShaderLoomRoot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
        drawPs2Backdrop(ImGui::GetWindowDrawList(), viewport->WorkPos, viewport->WorkSize, static_cast<float>(now), 0.72F, renderState.nebulaTexture);

        const float centerWidth = std::max(320.0F, viewport->WorkSize.x - LeftRailWidth - RightRailWidth);
        drawLeftRail(selectedEffect, imageState);
        ImGui::SameLine(0.0F, 0.0F);
        drawPreview(effects[selectedEffect], imageState, renderState, centerWidth, static_cast<float>(now));
        ImGui::SameLine(0.0F, 0.0F);
        drawSettingsRail(effects[selectedEffect], selectedEffect, imageState, renderState, appSettings);
        renderState.deferCpuEffectUpdate = isCpuEffect(selectedEffect) && ImGui::IsAnyItemActive();

        ImGui::End();

        ImGui::Render();
        int displayWidth = 0;
        int displayHeight = 0;
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.0F, 0.006F, 0.018F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    renderState.previewPipeline.reset();
    destroyRenderTextures(renderState);
    destroyTexture(imageState);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
