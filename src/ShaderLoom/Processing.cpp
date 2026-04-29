#include "ShaderLoom/Processing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

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

float mix(float lhs, float rhs, float amount) noexcept {
    return (lhs * (1.0F - amount)) + (rhs * amount);
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

Pixel sampleBilinear(const Image& image, float x, float y) noexcept {
    x = std::clamp(x, 0.0F, static_cast<float>(std::max(image.width() - 1, 0)));
    y = std::clamp(y, 0.0F, static_cast<float>(std::max(image.height() - 1, 0)));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, image.width() - 1);
    const int y1 = std::min(y0 + 1, image.height() - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);

    const Pixel a = image.pixel(x0, y0);
    const Pixel b = image.pixel(x1, y0);
    const Pixel c = image.pixel(x0, y1);
    const Pixel d = image.pixel(x1, y1);
    const float topR = mix(toFloat(a.r), toFloat(b.r), tx);
    const float topG = mix(toFloat(a.g), toFloat(b.g), tx);
    const float topB = mix(toFloat(a.b), toFloat(b.b), tx);
    const float bottomR = mix(toFloat(c.r), toFloat(d.r), tx);
    const float bottomG = mix(toFloat(c.g), toFloat(d.g), tx);
    const float bottomB = mix(toFloat(c.b), toFloat(d.b), tx);
    return floatPixel(mix(topR, bottomR, ty), mix(topG, bottomG, ty), mix(topB, bottomB, ty), a.a);
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

float fade(float value) noexcept {
    return value * value * value * (value * ((value * 6.0F) - 15.0F) + 10.0F);
}

float hashGrid(int x, int y) noexcept {
    auto value = static_cast<std::uint32_t>(x) * 0x8da6b343U;
    value ^= static_cast<std::uint32_t>(y) * 0xd8163841U;
    value ^= value >> 13U;
    value *= 0x85ebca6bU;
    value ^= value >> 16U;
    return static_cast<float>(value & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

int positiveModulo(int value, int period) noexcept {
    if (period <= 0) {
        return value;
    }
    const int wrapped = value % period;
    return wrapped < 0 ? wrapped + period : wrapped;
}

float hashGridTiled(int x, int y, int periodX, int periodY) noexcept {
    return hashGrid(positiveModulo(x, periodX), positiveModulo(y, periodY));
}

float valueNoise(float x, float y) noexcept {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(ix);
    const float fy = y - static_cast<float>(iy);
    const float u = fade(fx);
    const float v = fade(fy);

    const float a = hashGrid(ix, iy);
    const float b = hashGrid(ix + 1, iy);
    const float c = hashGrid(ix, iy + 1);
    const float d = hashGrid(ix + 1, iy + 1);
    return mix(mix(a, b, u), mix(c, d, u), v);
}

float valueNoiseTiled(float x, float y, int periodX, int periodY) noexcept {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(ix);
    const float fy = y - static_cast<float>(iy);
    const float u = fade(fx);
    const float v = fade(fy);

    const float a = hashGridTiled(ix, iy, periodX, periodY);
    const float b = hashGridTiled(ix + 1, iy, periodX, periodY);
    const float c = hashGridTiled(ix, iy + 1, periodX, periodY);
    const float d = hashGridTiled(ix + 1, iy + 1, periodX, periodY);
    return mix(mix(a, b, u), mix(c, d, u), v);
}

float fbmNoise(float x, float y) noexcept {
    float total = 0.0F;
    float amplitude = 0.55F;
    float normalizer = 0.0F;
    float frequency = 1.0F;
    for (int octave = 0; octave < 5; ++octave) {
        total += ((valueNoise(x * frequency, y * frequency) * 2.0F) - 1.0F) * amplitude;
        normalizer += amplitude;
        amplitude *= 0.5F;
        frequency *= 2.17F;
    }
    return total / std::max(normalizer, 0.001F);
}

float fbmNoiseTiled(float x, float y, int periodX, int periodY) noexcept {
    float total = 0.0F;
    float amplitude = 0.55F;
    float normalizer = 0.0F;
    float frequency = 1.0F;
    for (int octave = 0; octave < 5; ++octave) {
        const int octavePeriodX = std::max(1, static_cast<int>(std::round(static_cast<float>(periodX) * frequency)));
        const int octavePeriodY = std::max(1, static_cast<int>(std::round(static_cast<float>(periodY) * frequency)));
        total += ((valueNoiseTiled(x * frequency, y * frequency, octavePeriodX, octavePeriodY) * 2.0F) - 1.0F) * amplitude;
        normalizer += amplitude;
        amplitude *= 0.5F;
        frequency *= 2.0F;
    }
    return total / std::max(normalizer, 0.001F);
}

std::pair<float, float> directionVector(float angleDegrees) noexcept {
    const float angle = angleDegrees * Pi / 180.0F;
    return {std::cos(angle), -std::sin(angle)};
}

int loopPeriodUnits(const RenderContext& context, float scale, float speed) noexcept {
    if (!context.seamlessLoop || speed <= 0.001F) {
        return 0;
    }

    const float duration = std::max(context.loopDurationSeconds, 0.001F);
    const float desiredTravelUnits = (speed * 90.0F * duration) / std::max(scale, 0.001F);
    return std::max(1, static_cast<int>(std::round(desiredTravelUnits)));
}

std::pair<float, float> noiseFieldMotion(const RenderContext& context, float scale, float speed) noexcept {
    const auto [directionX, directionY] = directionVector(context.processing.noiseFieldAngleDegrees);
    if (context.seamlessLoop) {
        const int periodUnits = loopPeriodUnits(context, scale, speed);
        if (periodUnits <= 0) {
            return {0.0F, 0.0F};
        }

        const float duration = std::max(context.loopDurationSeconds, 0.001F);
        const float phase = std::fmod(std::max(context.timeSeconds, 0.0F), duration) / duration;
        const float drift = static_cast<float>(periodUnits) * scale * phase;
        return {directionX * drift, directionY * drift};
    }

    const float drift = context.timeSeconds * speed * 90.0F;
    return {directionX * drift, directionY * drift};
}

float noiseAt(float x, float y, int periodX, int periodY) noexcept {
    if (periodX <= 0 && periodY <= 0) {
        return valueNoise(x, y);
    }
    return valueNoiseTiled(x, y, periodX, periodY);
}

float fbmAt(float x, float y, int periodX, int periodY) noexcept {
    if (periodX <= 0 && periodY <= 0) {
        return fbmNoise(x, y);
    }
    return fbmNoiseTiled(x, y, periodX, periodY);
}

float noiseFieldAmount(int x, int y, const RenderContext& context) noexcept {
    const auto& processing = context.processing;
    if (!processing.noiseField || processing.noiseFieldStrength <= 0.001F) {
        return 0.0F;
    }

    const float scale = std::clamp(processing.noiseFieldScale, 4.0F, 120.0F);
    const float speed = std::clamp(processing.noiseFieldSpeed, 0.0F, 4.0F);
    const auto [motionX, motionY] = noiseFieldMotion(context, scale, speed);
    const float sampleX = static_cast<float>(x) - motionX;
    const float sampleY = static_cast<float>(y) - motionY;
    const auto [directionX, directionY] = directionVector(processing.noiseFieldAngleDegrees);
    const float perpendicularX = -directionY;
    const float perpendicularY = directionX;
    const int period = loopPeriodUnits(context, scale, speed);
    const float baseX = ((sampleX * directionX) + (sampleY * directionY)) / scale;
    const float baseY = ((sampleX * perpendicularX) + (sampleY * perpendicularY)) / scale;
    const float warpX = (noiseAt((baseX * 0.35F) + 17.3F, (baseY * 0.35F) - 9.7F, period, 0) * 2.0F) - 1.0F;
    const float warpY = (noiseAt((baseX * 0.35F) - 41.1F, (baseY * 0.35F) + 23.8F, period, 0) * 2.0F) - 1.0F;
    const float field = fbmAt(baseX + (warpX * 0.75F), baseY + (warpY * 0.75F), period, 0);
    const float fineScale = std::max(scale * 0.22F, 2.0F);
    const int finePeriod = period > 0 ? std::max(1, static_cast<int>(std::round((static_cast<float>(period) * scale) / fineScale))) : 0;
    const float fineX = ((sampleX * directionX) + (sampleY * directionY)) / fineScale;
    const float fineY = ((sampleX * perpendicularX) + (sampleY * perpendicularY)) / fineScale;
    const float fine = (noiseAt(fineX, fineY, finePeriod, 0) * 2.0F) - 1.0F;
    const float strength = std::clamp(processing.noiseFieldStrength, 0.0F, 1.0F);
    return ((field * 0.82F) + (fine * 0.18F)) * strength * 0.22F;
}

std::pair<float, float> noiseFieldVector(int x, int y, const RenderContext& context) noexcept {
    const auto& processing = context.processing;
    if (!processing.noiseField || processing.noiseFieldDistortion <= 0.001F) {
        return {0.0F, 0.0F};
    }

    const float scale = std::clamp(processing.noiseFieldScale, 4.0F, 120.0F);
    const float speed = std::clamp(processing.noiseFieldSpeed, 0.0F, 4.0F);
    const auto [motionX, motionY] = noiseFieldMotion(context, scale, speed);
    const float sampleX = static_cast<float>(x) - motionX;
    const float sampleY = static_cast<float>(y) - motionY;
    const auto [directionX, directionY] = directionVector(processing.noiseFieldAngleDegrees);
    const float perpendicularX = -directionY;
    const float perpendicularY = directionX;
    const int period = loopPeriodUnits(context, scale, speed);
    const float baseX = ((sampleX * directionX) + (sampleY * directionY)) / scale;
    const float baseY = ((sampleX * perpendicularX) + (sampleY * perpendicularY)) / scale;
    const float warpX = (noiseAt((baseX * 0.35F) + 17.3F, (baseY * 0.35F) - 9.7F, period, 0) * 2.0F) - 1.0F;
    const float warpY = (noiseAt((baseX * 0.35F) - 41.1F, (baseY * 0.35F) + 23.8F, period, 0) * 2.0F) - 1.0F;
    const float fieldX = fbmAt(baseX + (warpX * 0.75F) + 31.7F, baseY + (warpY * 0.75F) - 12.4F, period, 0);
    const float fieldY = fbmAt(baseX + (warpX * 0.75F) - 18.6F, baseY + (warpY * 0.75F) + 46.2F, period, 0);
    const float amount = std::clamp(processing.noiseFieldDistortion, 0.0F, 80.0F);
    return {fieldX * amount, fieldY * amount};
}

Image applyNoiseFieldDistortion(const Image& image, const RenderContext& context) {
    if (!context.processing.noiseField || context.processing.noiseFieldDistortion <= 0.001F) {
        return image;
    }

    Image output(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto [offsetX, offsetY] = noiseFieldVector(x, y, context);
            output.setPixel(x, y, sampleBilinear(image, static_cast<float>(x) + offsetX, static_cast<float>(y) + offsetY));
        }
    }
    return output;
}

Image applyNoiseField(const Image& image, const RenderContext& context) {
    if (!context.processing.noiseField || context.processing.noiseFieldStrength <= 0.001F) {
        return image;
    }

    Image output(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const Pixel pixel = image.pixel(x, y);
            const float field = noiseFieldAmount(x, y, context);
            output.setPixel(
                x,
                y,
                floatPixel(toFloat(pixel.r) + field, toFloat(pixel.g) + field, toFloat(pixel.b) + field, pixel.a)
            );
        }
    }
    return output;
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
    const Image source = applyNoiseFieldDistortion(image, context);
    Image output(source.width(), source.height());
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            output.setPixel(x, y, applyProcessing(source.pixel(x, y), context));
        }
    }

    output = applyBlur(output, context.processing.blur);
    output = applySharpen(output, context.adjustments.sharpness);
    output = applyNoiseField(output, context);
    output = applyEdgeEnhance(output, context.processing.edgeEnhance);
    return output;
}

} // namespace ShaderLoom
