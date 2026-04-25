#pragma once

#include "ShaderLoom/Image.hpp"
#include "ShaderLoom/Processing.hpp"

namespace ShaderLoom {

enum class PixelSortDirection {
    Horizontal,
    Vertical,
    Diagonal
};

enum class PixelSortMode {
    Brightness,
    Hue,
    Saturation
};

struct PixelSortSettings {
    PixelSortDirection direction = PixelSortDirection::Horizontal;
    PixelSortMode sortMode = PixelSortMode::Brightness;
    float threshold = 0.2F;
    int streakLength = 110;
    float intensity = 0.7F;
    float randomness = 0.0F;
    bool reverse = false;
};

class PixelSortEffect {
public:
    [[nodiscard]] Image apply(const Image& source, const PixelSortSettings& settings, const RenderContext& context) const;
};

} // namespace ShaderLoom
