#include "ShaderLoom/effects/DitherEffect.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ShaderLoom {
namespace {

struct DiffusionWeight {
    int dx = 0;
    int dy = 0;
    float weight = 0.0F;
};

float toFloat(std::uint8_t value) noexcept {
    return static_cast<float>(value) / 255.0F;
}

std::uint8_t toByte(float value) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

std::vector<DiffusionWeight> diffusionKernel(DitherAlgorithm algorithm) {
    switch (algorithm) {
    case DitherAlgorithm::Atkinson:
        return {
            {1, 0, 1.0F / 8.0F}, {2, 0, 1.0F / 8.0F},
            {-1, 1, 1.0F / 8.0F}, {0, 1, 1.0F / 8.0F}, {1, 1, 1.0F / 8.0F},
            {0, 2, 1.0F / 8.0F}
        };
    case DitherAlgorithm::JarvisJudiceNinke:
        return {
            {1, 0, 7.0F / 48.0F}, {2, 0, 5.0F / 48.0F},
            {-2, 1, 3.0F / 48.0F}, {-1, 1, 5.0F / 48.0F}, {0, 1, 7.0F / 48.0F}, {1, 1, 5.0F / 48.0F}, {2, 1, 3.0F / 48.0F},
            {-2, 2, 1.0F / 48.0F}, {-1, 2, 3.0F / 48.0F}, {0, 2, 5.0F / 48.0F}, {1, 2, 3.0F / 48.0F}, {2, 2, 1.0F / 48.0F}
        };
    case DitherAlgorithm::Stucki:
        return {
            {1, 0, 8.0F / 42.0F}, {2, 0, 4.0F / 42.0F},
            {-2, 1, 2.0F / 42.0F}, {-1, 1, 4.0F / 42.0F}, {0, 1, 8.0F / 42.0F}, {1, 1, 4.0F / 42.0F}, {2, 1, 2.0F / 42.0F},
            {-2, 2, 1.0F / 42.0F}, {-1, 2, 2.0F / 42.0F}, {0, 2, 4.0F / 42.0F}, {1, 2, 2.0F / 42.0F}, {2, 2, 1.0F / 42.0F}
        };
    case DitherAlgorithm::Burkes:
        return {
            {1, 0, 8.0F / 32.0F}, {2, 0, 4.0F / 32.0F},
            {-2, 1, 2.0F / 32.0F}, {-1, 1, 4.0F / 32.0F}, {0, 1, 8.0F / 32.0F}, {1, 1, 4.0F / 32.0F}, {2, 1, 2.0F / 32.0F}
        };
    case DitherAlgorithm::Sierra:
        return {
            {1, 0, 5.0F / 32.0F}, {2, 0, 3.0F / 32.0F},
            {-2, 1, 2.0F / 32.0F}, {-1, 1, 4.0F / 32.0F}, {0, 1, 5.0F / 32.0F}, {1, 1, 4.0F / 32.0F}, {2, 1, 2.0F / 32.0F},
            {-1, 2, 2.0F / 32.0F}, {0, 2, 3.0F / 32.0F}, {1, 2, 2.0F / 32.0F}
        };
    case DitherAlgorithm::SierraTwoRow:
        return {
            {1, 0, 4.0F / 16.0F}, {2, 0, 3.0F / 16.0F},
            {-2, 1, 1.0F / 16.0F}, {-1, 1, 2.0F / 16.0F}, {0, 1, 3.0F / 16.0F}, {1, 1, 2.0F / 16.0F}, {2, 1, 1.0F / 16.0F}
        };
    case DitherAlgorithm::SierraLite:
        return {
            {1, 0, 2.0F / 4.0F},
            {-1, 1, 1.0F / 4.0F}, {0, 1, 1.0F / 4.0F}
        };
    case DitherAlgorithm::FloydSteinberg:
    case DitherAlgorithm::Bayer2x2:
    default:
        return {
            {1, 0, 7.0F / 16.0F},
            {-1, 1, 3.0F / 16.0F}, {0, 1, 5.0F / 16.0F}, {1, 1, 1.0F / 16.0F}
        };
    }
}

} // namespace

Image DitherEffect::apply(const Image& source, const DitherSettings& settings, const RenderContext& context) const {
    const Image processed = applyProcessing(source, context);
    if (settings.algorithm == DitherAlgorithm::Bayer2x2) {
        Image output(processed.width(), processed.height());
        const float blend = std::clamp(settings.intensity, 0.0F, 1.0F);
        constexpr float matrix[2][2] = {
            {0.25F, 0.75F},
            {1.0F, 0.5F}
        };

        for (int y = 0; y < processed.height(); ++y) {
            for (int x = 0; x < processed.width(); ++x) {
                const Pixel original = processed.pixel(x, y);
                const float threshold = matrix[y % 2][x % 2];
                const float mask = luminance(original) > threshold ? 1.0F : 0.0F;
                const float r = toFloat(original.r) * ((1.0F - blend) + (mask * blend));
                const float g = toFloat(original.g) * ((1.0F - blend) + (mask * blend));
                const float b = toFloat(original.b) * ((1.0F - blend) + (mask * blend));
                output.setPixel(x, y, Pixel{toByte(r), toByte(g), toByte(b), original.a});
            }
        }

        return output;
    }

    std::vector<float> work(static_cast<std::size_t>(processed.width() * processed.height()));

    const std::vector<DiffusionWeight> kernel = diffusionKernel(settings.algorithm);

    for (int y = 0; y < processed.height(); ++y) {
        for (int x = 0; x < processed.width(); ++x) {
            const Pixel pixel = processed.pixel(x, y);
            work[static_cast<std::size_t>(y * processed.width() + x)] = luminance(pixel);
        }
    }

    for (int y = 0; y < processed.height(); ++y) {
        for (int x = 0; x < processed.width(); ++x) {
            float& oldTone = work[static_cast<std::size_t>(y * processed.width() + x)];
            const float newTone = oldTone >= 0.5F ? 1.0F : 0.0F;
            const float error = oldTone - newTone;
            oldTone = newTone;

            for (const DiffusionWeight& weight : kernel) {
                const int targetX = x + weight.dx;
                const int targetY = y + weight.dy;
                if (targetX >= 0 && targetY >= 0 && targetX < processed.width() && targetY < processed.height()) {
                    work[static_cast<std::size_t>(targetY * processed.width() + targetX)] += error * weight.weight;
                }
            }
        }
    }

    Image output(processed.width(), processed.height());
    const float blend = std::clamp(settings.intensity, 0.0F, 1.0F);

    for (int y = 0; y < processed.height(); ++y) {
        for (int x = 0; x < processed.width(); ++x) {
            const Pixel original = processed.pixel(x, y);
            const float mask = work[static_cast<std::size_t>(y * processed.width() + x)];
            const float r = toFloat(original.r) * ((1.0F - blend) + (mask * blend));
            const float g = toFloat(original.g) * ((1.0F - blend) + (mask * blend));
            const float b = toFloat(original.b) * ((1.0F - blend) + (mask * blend));
            output.setPixel(x, y, Pixel{toByte(r), toByte(g), toByte(b), original.a});
        }
    }

    return output;
}

} // namespace ShaderLoom
