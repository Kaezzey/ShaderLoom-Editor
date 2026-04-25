#include "grainrad/Processing.hpp"

#include <algorithm>
#include <cmath>

namespace grainrad {
namespace {

float toFloat(std::uint8_t value) noexcept {
    return static_cast<float>(value) / 255.0F;
}

std::uint8_t toByte(float value) noexcept {
    value = std::clamp(value, 0.0F, 1.0F);
    return static_cast<std::uint8_t>(std::lround(value * 255.0F));
}

} // namespace

float luminance(Pixel pixel) noexcept {
    return (0.299F * toFloat(pixel.r)) + (0.587F * toFloat(pixel.g)) + (0.114F * toFloat(pixel.b));
}

Pixel applyProcessing(Pixel pixel, const RenderContext& context) {
    float r = toFloat(pixel.r);
    float g = toFloat(pixel.g);
    float b = toFloat(pixel.b);

    if (context.processing.invert) {
        r = 1.0F - r;
        g = 1.0F - g;
        b = 1.0F - b;
    }

    r += context.adjustments.brightness / 100.0F;
    g += context.adjustments.brightness / 100.0F;
    b += context.adjustments.brightness / 100.0F;

    const float contrastFactor = 1.0F + (context.adjustments.contrast / 100.0F);
    r = ((r - 0.5F) * contrastFactor) + 0.5F;
    g = ((g - 0.5F) * contrastFactor) + 0.5F;
    b = ((b - 0.5F) * contrastFactor) + 0.5F;

    const float gray = (0.299F * r) + (0.587F * g) + (0.114F * b);
    const float saturationFactor = 1.0F + (context.adjustments.saturation / 100.0F);
    r = gray + ((r - gray) * saturationFactor);
    g = gray + ((g - gray) * saturationFactor);
    b = gray + ((b - gray) * saturationFactor);

    const float gamma = std::max(context.adjustments.gamma, 0.01F);
    r = std::pow(std::clamp(r, 0.0F, 1.0F), 1.0F / gamma);
    g = std::pow(std::clamp(g, 0.0F, 1.0F), 1.0F / gamma);
    b = std::pow(std::clamp(b, 0.0F, 1.0F), 1.0F / gamma);

    if (context.processing.brightnessMap != 1.0F) {
        const float currentLuma = std::max((0.299F * r) + (0.587F * g) + (0.114F * b), 0.001F);
        const float mappedLuma = std::pow(currentLuma, context.processing.brightnessMap);
        const float scale = mappedLuma / currentLuma;
        r *= scale;
        g *= scale;
        b *= scale;
    }

    if (context.processing.quantizeColors > 1) {
        const float levels = static_cast<float>(context.processing.quantizeColors - 1);
        r = std::round(std::clamp(r, 0.0F, 1.0F) * levels) / levels;
        g = std::round(std::clamp(g, 0.0F, 1.0F) * levels) / levels;
        b = std::round(std::clamp(b, 0.0F, 1.0F) * levels) / levels;
    }

    return Pixel{toByte(r), toByte(g), toByte(b), pixel.a};
}

Image applyProcessing(const Image& image, const RenderContext& context) {
    Image output(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            output.setPixel(x, y, applyProcessing(image.pixel(x, y), context));
        }
    }
    return output;
}

} // namespace grainrad
