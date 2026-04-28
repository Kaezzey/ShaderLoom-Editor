#include "ShaderLoom/effects/PixelSortEffect.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace ShaderLoom {
namespace {

std::uint8_t blendChannel(std::uint8_t original, std::uint8_t sorted, float intensity) {
    const float mixed = (static_cast<float>(original) * (1.0F - intensity)) + (static_cast<float>(sorted) * intensity);
    return static_cast<std::uint8_t>(std::lround(std::clamp(mixed, 0.0F, 255.0F)));
}

float channelToFloat(std::uint8_t channel) noexcept {
    return static_cast<float>(channel) / 255.0F;
}

float saturation(Pixel pixel) noexcept {
    const float r = channelToFloat(pixel.r);
    const float g = channelToFloat(pixel.g);
    const float b = channelToFloat(pixel.b);
    const float maxValue = std::max({r, g, b});
    const float minValue = std::min({r, g, b});
    if (maxValue <= 0.0F) {
        return 0.0F;
    }
    return (maxValue - minValue) / maxValue;
}

float hue(Pixel pixel) noexcept {
    const float r = channelToFloat(pixel.r);
    const float g = channelToFloat(pixel.g);
    const float b = channelToFloat(pixel.b);
    const float maxValue = std::max({r, g, b});
    const float minValue = std::min({r, g, b});
    const float delta = maxValue - minValue;
    if (delta <= 0.0001F) {
        return 0.0F;
    }

    float h = 0.0F;
    if (maxValue == r) {
        h = std::fmod((g - b) / delta, 6.0F);
    } else if (maxValue == g) {
        h = ((b - r) / delta) + 2.0F;
    } else {
        h = ((r - g) / delta) + 4.0F;
    }
    h /= 6.0F;
    return h < 0.0F ? h + 1.0F : h;
}

float sortValue(Pixel pixel, PixelSortMode mode) noexcept {
    if (mode == PixelSortMode::Hue) {
        return hue(pixel);
    }
    if (mode == PixelSortMode::Saturation) {
        return saturation(pixel);
    }
    return luminance(pixel);
}

float hash01(int x, int y, int seed) noexcept {
    std::uint32_t value = static_cast<std::uint32_t>(x) * 374761393U;
    value += static_cast<std::uint32_t>(y) * 668265263U;
    value += static_cast<std::uint32_t>(seed) * 2246822519U;
    value = (value ^ (value >> 13U)) * 1274126177U;
    value ^= value >> 16U;
    return static_cast<float>(value & 0x00FFFFFFU) / static_cast<float>(0x00FFFFFFU);
}

bool isActive(Pixel pixel, const PixelSortSettings& settings, float localThreshold) noexcept {
    return sortValue(pixel, settings.sortMode) >= localThreshold;
}

int chunkLength(
    int x,
    int y,
    int runOffset,
    int remaining,
    const PixelSortSettings& settings
) noexcept {
    const int baseLength = std::max(1, settings.streakLength);
    const float randomness = std::clamp(settings.randomness, 0.0F, 1.0F);
    if (randomness <= 0.0001F) {
        return std::min(baseLength, remaining);
    }

    const float jitter = hash01(x + 53, y + 97, runOffset) - 0.5F;
    const float scale = std::clamp(1.0F + (jitter * randomness * 1.6F), 0.25F, 2.0F);
    return std::clamp(static_cast<int>(std::round(static_cast<float>(baseLength) * scale)), 1, remaining);
}

void sortRun(
    const Image& processed,
    Image& sorted,
    const PixelSortSettings& settings,
    const std::vector<std::pair<int, int>>& points,
    int start,
    int end
) {
    int index = start;
    while (index < end) {
        const auto [x, y] = points[static_cast<std::size_t>(index)];
        const int length = chunkLength(x, y, index - start, end - index, settings);
        const int chunkEnd = std::min(index + length, end);

        std::vector<Pixel> segment;
        segment.reserve(static_cast<std::size_t>(chunkEnd - index));
        for (int i = index; i < chunkEnd; ++i) {
            const auto [sx, sy] = points[static_cast<std::size_t>(i)];
            segment.push_back(processed.pixel(sx, sy));
        }

        std::sort(segment.begin(), segment.end(), [&settings](Pixel lhs, Pixel rhs) {
            return sortValue(lhs, settings.sortMode) < sortValue(rhs, settings.sortMode);
        });

        if (settings.reverse) {
            std::reverse(segment.begin(), segment.end());
        }

        for (int i = index; i < chunkEnd; ++i) {
            const auto [sx, sy] = points[static_cast<std::size_t>(i)];
            sorted.setPixel(sx, sy, segment[static_cast<std::size_t>(i - index)]);
        }

        index = chunkEnd;
    }
}

void sortLine(
    const Image& processed,
    Image& sorted,
    const PixelSortSettings& settings,
    const std::vector<std::pair<int, int>>& points
) {
    const float baseThreshold = std::clamp(settings.threshold, 0.0F, 1.0F);
    const float randomness = std::clamp(settings.randomness, 0.0F, 1.0F);
    int index = 0;

    while (index < static_cast<int>(points.size())) {
        const auto [x, y] = points[static_cast<std::size_t>(index)];
        const float thresholdNoise = hash01(x + 11, y + 23, index) - 0.5F;
        const float localThreshold = std::clamp(baseThreshold + (thresholdNoise * randomness * 0.35F), 0.0F, 1.0F);

        if (!isActive(processed.pixel(x, y), settings, localThreshold)) {
            ++index;
            continue;
        }

        const int start = index;
        while (index < static_cast<int>(points.size())) {
            const auto [sx, sy] = points[static_cast<std::size_t>(index)];
            if (!isActive(processed.pixel(sx, sy), settings, localThreshold)) {
                break;
            }
            ++index;
        }

        sortRun(processed, sorted, settings, points, start, index);
    }
}

} // namespace

Image PixelSortEffect::apply(const Image& source, const PixelSortSettings& settings, const RenderContext& context) const {
    const Image processed = applyProcessing(source, context);
    Image sorted = processed;

    if (settings.direction == PixelSortDirection::Vertical) {
        for (int x = 0; x < processed.width(); ++x) {
            std::vector<std::pair<int, int>> points;
            points.reserve(static_cast<std::size_t>(processed.height()));
            for (int y = 0; y < processed.height(); ++y) {
                points.emplace_back(x, y);
            }
            sortLine(processed, sorted, settings, points);
        }
    } else if (settings.direction == PixelSortDirection::Diagonal) {
        for (int startX = 0; startX < processed.width(); ++startX) {
            std::vector<std::pair<int, int>> points;
            for (int x = startX, y = 0; x < processed.width() && y < processed.height(); ++x, ++y) {
                points.emplace_back(x, y);
            }
            sortLine(processed, sorted, settings, points);
        }
        for (int startY = 1; startY < processed.height(); ++startY) {
            std::vector<std::pair<int, int>> points;
            for (int x = 0, y = startY; x < processed.width() && y < processed.height(); ++x, ++y) {
                points.emplace_back(x, y);
            }
            sortLine(processed, sorted, settings, points);
        }
    } else {
        for (int y = 0; y < processed.height(); ++y) {
            std::vector<std::pair<int, int>> points;
            points.reserve(static_cast<std::size_t>(processed.width()));
            for (int x = 0; x < processed.width(); ++x) {
                points.emplace_back(x, y);
            }
            sortLine(processed, sorted, settings, points);
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
