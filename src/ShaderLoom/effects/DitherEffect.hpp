#pragma once

#include "ShaderLoom/Image.hpp"
#include "ShaderLoom/Processing.hpp"

namespace ShaderLoom {

enum class DitherAlgorithm {
    FloydSteinberg,
    Atkinson,
    JarvisJudiceNinke,
    Stucki,
    Burkes,
    Sierra,
    SierraTwoRow,
    SierraLite,
    Bayer2x2
};

struct DitherSettings {
    DitherAlgorithm algorithm = DitherAlgorithm::FloydSteinberg;
    float intensity = 0.1F;
    bool modulation = false;
};

class DitherEffect {
public:
    [[nodiscard]] Image apply(const Image& source, const DitherSettings& settings, const RenderContext& context) const;
};

} // namespace ShaderLoom
