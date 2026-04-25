#pragma once

#include "grainrad/Image.hpp"
#include "grainrad/Processing.hpp"

namespace grainrad {

struct PixelSortSettings {
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

} // namespace grainrad
