#include "ShaderLoom/effects/PixelSortEffect.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ShaderLoom {
namespace {

std::uint8_t blendChannel(std::uint8_t original, std::uint8_t sorted, float intensity) {
    const float mixed = (static_cast<float>(original) * (1.0F - intensity)) + (static_cast<float>(sorted) * intensity);
    return static_cast<std::uint8_t>(std::lround(std::clamp(mixed, 0.0F, 255.0F)));
}

} // namespace

Image PixelSortEffect::apply(const Image& source, const PixelSortSettings& settings, const RenderContext& context) const {
    const Image processed = applyProcessing(source, context);
    Image sorted = processed;

    const float threshold = std::clamp(settings.threshold, 0.0F, 1.0F);
    const int maxStreak = std::max(1, settings.streakLength);

    for (int y = 0; y < processed.height(); ++y) {
        int x = 0;
        while (x < processed.width()) {
            if (luminance(processed.pixel(x, y)) <= threshold) {
                ++x;
                continue;
            }

            const int start = x;
            while (x < processed.width() && luminance(processed.pixel(x, y)) > threshold && (x - start) < maxStreak) {
                ++x;
            }
            const int end = x;

            std::vector<Pixel> segment;
            segment.reserve(static_cast<std::size_t>(end - start));
            for (int sx = start; sx < end; ++sx) {
                segment.push_back(processed.pixel(sx, y));
            }

            std::sort(segment.begin(), segment.end(), [](Pixel lhs, Pixel rhs) {
                return luminance(lhs) < luminance(rhs);
            });

            if (settings.reverse) {
                std::reverse(segment.begin(), segment.end());
            }

            for (int sx = start; sx < end; ++sx) {
                sorted.setPixel(sx, y, segment[static_cast<std::size_t>(sx - start)]);
            }
        }
    }

    Image output(processed.width(), processed.height());
    const float intensity = std::clamp(settings.intensity, 0.0F, 1.0F);
    for (int y = 0; y < processed.height(); ++y) {
        for (int x = 0; x < processed.width(); ++x) {
            const Pixel original = processed.pixel(x, y);
            const Pixel sortedPixel = sorted.pixel(x, y);
            output.setPixel(x, y, Pixel{
                blendChannel(original.r, sortedPixel.r, intensity),
                blendChannel(original.g, sortedPixel.g, intensity),
                blendChannel(original.b, sortedPixel.b, intensity),
                original.a
            });
        }
    }

    return output;
}

} // namespace ShaderLoom
