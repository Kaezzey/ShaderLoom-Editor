#include "ShaderLoom/effects/DitherEffect.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ShaderLoom {
namespace {

struct FloatPixel {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

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

void diffuse(std::vector<FloatPixel>& pixels, int width, int height, int x, int y, float er, float eg, float eb, float factor) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    FloatPixel& target = pixels[static_cast<std::size_t>(y * width + x)];
    target.r += er * factor;
    target.g += eg * factor;
    target.b += eb * factor;
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
                const float rDither = toFloat(original.r) > threshold ? 1.0F : 0.0F;
                const float gDither = toFloat(original.g) > threshold ? 1.0F : 0.0F;
                const float bDither = toFloat(original.b) > threshold ? 1.0F : 0.0F;
                const float r = (toFloat(original.r) * (1.0F - blend)) + (rDither * blend);
                const float g = (toFloat(original.g) * (1.0F - blend)) + (gDither * blend);
                const float b = (toFloat(original.b) * (1.0F - blend)) + (bDither * blend);
                output.setPixel(x, y, Pixel{toByte(r), toByte(g), toByte(b), original.a});
            }
        }

        return output;
    }

    std::vector<FloatPixel> work(static_cast<std::size_t>(processed.width() * processed.height()));

    const std::vector<DiffusionWeight> kernel = diffusionKernel(settings.algorithm);

    for (int y = 0; y < processed.height(); ++y) {
        for (int x = 0; x < processed.width(); ++x) {
            const Pixel pixel = processed.pixel(x, y);
            work[static_cast<std::size_t>(y * processed.width() + x)] = FloatPixel{
                toFloat(pixel.r),
                toFloat(pixel.g),
                toFloat(pixel.b),
                toFloat(pixel.a)
            };
        }
    }

    for (int y = 0; y < processed.height(); ++y) {
        for (int x = 0; x < processed.width(); ++x) {
            FloatPixel& oldPixel = work[static_cast<std::size_t>(y * processed.width() + x)];
            const float newR = oldPixel.r >= 0.5F ? 1.0F : 0.0F;
            const float newG = oldPixel.g >= 0.5F ? 1.0F : 0.0F;
            const float newB = oldPixel.b >= 0.5F ? 1.0F : 0.0F;

            const float er = oldPixel.r - newR;
            const float eg = oldPixel.g - newG;
            const float eb = oldPixel.b - newB;

            oldPixel.r = newR;
            oldPixel.g = newG;
            oldPixel.b = newB;

            for (const DiffusionWeight& weight : kernel) {
                diffuse(work, processed.width(), processed.height(), x + weight.dx, y + weight.dy, er, eg, eb, weight.weight);
            }
        }
    }

    Image output(processed.width(), processed.height());
    const float blend = std::clamp(settings.intensity, 0.0F, 1.0F);

    for (int y = 0; y < processed.height(); ++y) {
        for (int x = 0; x < processed.width(); ++x) {
            const Pixel original = processed.pixel(x, y);
            const FloatPixel dithered = work[static_cast<std::size_t>(y * processed.width() + x)];
            const float r = (toFloat(original.r) * (1.0F - blend)) + (dithered.r * blend);
            const float g = (toFloat(original.g) * (1.0F - blend)) + (dithered.g * blend);
            const float b = (toFloat(original.b) * (1.0F - blend)) + (dithered.b * blend);
            output.setPixel(x, y, Pixel{toByte(r), toByte(g), toByte(b), original.a});
        }
    }

    return output;
}

} // namespace ShaderLoom
