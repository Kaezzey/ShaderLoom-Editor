#pragma once

#include "grainrad/Image.hpp"
#include "grainrad/Processing.hpp"

namespace grainrad {

struct DitherSettings {
    float intensity = 0.1F;
    bool modulation = false;
};

class DitherEffect {
public:
    [[nodiscard]] Image apply(const Image& source, const DitherSettings& settings, const RenderContext& context) const;
};

} // namespace grainrad
