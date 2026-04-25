#include "grainrad/effects/DitherEffect.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace grainrad {
namespace {

struct FloatPixel {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
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

} // namespace

Image DitherEffect::apply(const Image& source, const DitherSettings& settings, const RenderContext& context) const {
    const Image processed = applyProcessing(source, context);
    std::vector<FloatPixel> work(static_cast<std::size_t>(processed.width() * processed.height()));

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

            diffuse(work, processed.width(), processed.height(), x + 1, y, er, eg, eb, 7.0F / 16.0F);
            diffuse(work, processed.width(), processed.height(), x - 1, y + 1, er, eg, eb, 3.0F / 16.0F);
            diffuse(work, processed.width(), processed.height(), x, y + 1, er, eg, eb, 5.0F / 16.0F);
            diffuse(work, processed.width(), processed.height(), x + 1, y + 1, er, eg, eb, 1.0F / 16.0F);
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

} // namespace grainrad
