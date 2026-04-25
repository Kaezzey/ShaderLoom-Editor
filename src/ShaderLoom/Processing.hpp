#pragma once

#include "ShaderLoom/Image.hpp"

namespace ShaderLoom {

struct AdjustmentSettings {
    float brightness = 0.0F;
    float contrast = 0.0F;
    float saturation = 0.0F;
    float hueRotationDegrees = 0.0F;
    float sharpness = 0.0F;
    float gamma = 1.0F;
};

struct ProcessingSettings {
    bool invert = false;
    float brightnessMap = 1.0F;
    float edgeEnhance = 0.0F;
    float blur = 0.0F;
    int quantizeColors = 0;
    float shapeMatching = 0.0F;
};

struct RenderContext {
    AdjustmentSettings adjustments;
    ProcessingSettings processing;
};

[[nodiscard]] float luminance(Pixel pixel) noexcept;
[[nodiscard]] Pixel applyProcessing(Pixel pixel, const RenderContext& context);
[[nodiscard]] Image applyProcessing(const Image& image, const RenderContext& context);

} // namespace ShaderLoom
