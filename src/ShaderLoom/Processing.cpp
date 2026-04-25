#include "ShaderLoom/Processing.hpp"

#include <algorithm>
#include <cmath>

namespace ShaderLoom {
namespace {

constexpr float Pi = 3.14159265358979323846F;

float toFloat(std::uint8_t value) noexcept {
    return static_cast<float>(value) / 255.0F;
}

std::uint8_t toByte(float value) noexcept {
    value = std::clamp(value, 0.0F, 1.0F);
    return static_cast<std::uint8_t>(std::lround(value * 255.0F));
}

Pixel floatPixel(float r, float g, float b, std::uint8_t alpha) noexcept {
    return Pixel{toByte(r), toByte(g), toByte(b), alpha};
}

Pixel mixPixels(Pixel lhs, Pixel rhs, float amount) noexcept {
    amount = std::clamp(amount, 0.0F, 1.0F);
    const float keep = 1.0F - amount;
    return Pixel{
        toByte((toFloat(lhs.r) * keep) + (toFloat(rhs.r) * amount)),
        toByte((toFloat(lhs.g) * keep) + (toFloat(rhs.g) * amount)),
        toByte((toFloat(lhs.b) * keep) + (toFloat(rhs.b) * amount)),
        lhs.a
    };
}

void rotateHue(float& r, float& g, float& b, float degrees) noexcept {
    if (std::abs(degrees) <= 0.001F) {
        return;
    }

    const float angle = degrees * Pi / 180.0F;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float y = (0.299F * r) + (0.587F * g) + (0.114F * b);
    const float i = (0.596F * r) - (0.274F * g) - (0.322F * b);
    const float q = (0.211F * r) - (0.523F * g) + (0.312F * b);
    const float rotatedI = (i * c) - (q * s);
    const float rotatedQ = (i * s) + (q * c);

    r = y + (0.956F * rotatedI) + (0.621F * rotatedQ);
    g = y - (0.272F * rotatedI) - (0.647F * rotatedQ);
    b = y - (1.106F * rotatedI) + (1.703F * rotatedQ);
}

Pixel blurPixel(const Image& image, int x, int y, int radius) {
    float r = toFloat(image.pixel(x, y).r) * 4.0F;
    float g = toFloat(image.pixel(x, y).g) * 4.0F;
    float b = toFloat(image.pixel(x, y).b) * 4.0F;

    constexpr int Offsets[8][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
        {1, 1},
        {-1, -1},
        {1, -1},
        {-1, 1}
    };

    for (const auto& offset : Offsets) {
        const Pixel sample = image.pixel(x + (offset[0] * radius), y + (offset[1] * radius));
        r += toFloat(sample.r);
        g += toFloat(sample.g);
        b += toFloat(sample.b);
    }

    const Pixel center = image.pixel(x, y);
    return floatPixel(r / 12.0F, g / 12.0F, b / 12.0F, center.a);
}

Image applyBlur(const Image& image, float amount) {
    if (amount <= 0.001F) {
        return image;
    }

    Image output(image.width(), image.height());
    const int radius = std::max(1, static_cast<int>(std::round(amount)));
    const float mix = std::clamp(amount / 10.0F, 0.0F, 1.0F);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            output.setPixel(x, y, mixPixels(image.pixel(x, y), blurPixel(image, x, y, radius), mix));
        }
    }
    return output;
}

Image applySharpen(const Image& image, float amount) {
    if (amount <= 0.001F) {
        return image;
    }

    Image output(image.width(), image.height());
    const float mix = std::clamp(amount / 5.0F, 0.0F, 1.0F);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const Pixel center = image.pixel(x, y);
            float r = toFloat(center.r) * 5.0F;
            float g = toFloat(center.g) * 5.0F;
            float b = toFloat(center.b) * 5.0F;

            const Pixel right = image.pixel(x + 1, y);
            const Pixel left = image.pixel(x - 1, y);
            const Pixel down = image.pixel(x, y + 1);
            const Pixel up = image.pixel(x, y - 1);
            r -= toFloat(right.r) + toFloat(left.r) + toFloat(down.r) + toFloat(up.r);
            g -= toFloat(right.g) + toFloat(left.g) + toFloat(down.g) + toFloat(up.g);
            b -= toFloat(right.b) + toFloat(left.b) + toFloat(down.b) + toFloat(up.b);

            output.setPixel(x, y, mixPixels(center, floatPixel(r, g, b, center.a), mix));
        }
    }
    return output;
}

float sobelEdge(const Image& image, int x, int y) {
    const float tl = luminance(image.pixel(x - 1, y - 1));
    const float tc = luminance(image.pixel(x, y - 1));
    const float tr = luminance(image.pixel(x + 1, y - 1));
    const float ml = luminance(image.pixel(x - 1, y));
    const float mr = luminance(image.pixel(x + 1, y));
    const float bl = luminance(image.pixel(x - 1, y + 1));
    const float bc = luminance(image.pixel(x, y + 1));
    const float br = luminance(image.pixel(x + 1, y + 1));
    const float gx = -tl - (2.0F * ml) - bl + tr + (2.0F * mr) + br;
    const float gy = -tl - (2.0F * tc) - tr + bl + (2.0F * bc) + br;
    return std::clamp(std::sqrt((gx * gx) + (gy * gy)), 0.0F, 1.0F);
}

Image applyEdgeEnhance(const Image& image, float amount) {
    if (amount <= 0.001F) {
        return image;
    }

    Image output(image.width(), image.height());
    const float scale = std::clamp(amount / 5.0F, 0.0F, 1.0F);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const Pixel pixel = image.pixel(x, y);
            const float edge = sobelEdge(image, x, y) * scale;
            output.setPixel(
                x,
                y,
                floatPixel(toFloat(pixel.r) + edge, toFloat(pixel.g) + edge, toFloat(pixel.b) + edge, pixel.a)
            );
        }
    }
    return output;
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

    rotateHue(r, g, b, context.adjustments.hueRotationDegrees);
    r = std::clamp(r, 0.0F, 1.0F);
    g = std::clamp(g, 0.0F, 1.0F);
    b = std::clamp(b, 0.0F, 1.0F);

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

    if (context.processing.shapeMatching > 0.001F) {
        const float currentLuma = std::max((0.299F * r) + (0.587F * g) + (0.114F * b), 0.001F);
        const float shaped = std::round(std::clamp((currentLuma - 0.05F) / 0.90F, 0.0F, 1.0F) * 5.0F) / 5.0F;
        const float scale = shaped / currentLuma;
        const float amount = std::clamp(context.processing.shapeMatching, 0.0F, 1.0F);
        r = (r * (1.0F - amount)) + (r * scale * amount);
        g = (g * (1.0F - amount)) + (g * scale * amount);
        b = (b * (1.0F - amount)) + (b * scale * amount);
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

    output = applyBlur(output, context.processing.blur);
    output = applySharpen(output, context.adjustments.sharpness);
    output = applyEdgeEnhance(output, context.processing.edgeEnhance);
    return output;
}

} // namespace ShaderLoom
