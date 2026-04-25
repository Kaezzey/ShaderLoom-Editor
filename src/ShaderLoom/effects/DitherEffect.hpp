#pragma once

#include "ShaderLoom/Image.hpp"
#include "ShaderLoom/Processing.hpp"

namespace ShaderLoom {

struct DitherSettings {
    float intensity = 0.1F;
    bool modulation = false;
};

class DitherEffect {
public:
    [[nodiscard]] Image apply(const Image& source, const DitherSettings& settings, const RenderContext& context) const;
};

} // namespace ShaderLoom
