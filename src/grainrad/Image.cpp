#include "grainrad/Image.hpp"

#include <algorithm>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace grainrad {

Image::Image(int width, int height)
    : width_(width), height_(height), rgba_(static_cast<std::size_t>(width * height * 4), 0) {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            setPixel(x, y, Pixel{0, 0, 0, 255});
        }
    }
}

Image::Image(int width, int height, std::vector<std::uint8_t> rgba)
    : width_(width), height_(height), rgba_(std::move(rgba)) {
    if (width_ <= 0 || height_ <= 0) {
        throw std::runtime_error("Image dimensions must be positive.");
    }
    if (rgba_.size() != static_cast<std::size_t>(width_ * height_ * 4)) {
        throw std::runtime_error("Image pixel buffer must contain RGBA data.");
    }
}

Image Image::load(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* loaded = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!loaded) {
        throw std::runtime_error("Failed to load image: " + path.string());
    }

    std::vector<std::uint8_t> rgba(
        loaded,
        loaded + static_cast<std::size_t>(width * height * 4)
    );
    stbi_image_free(loaded);
    return Image(width, height, std::move(rgba));
}

bool Image::empty() const noexcept {
    return width_ <= 0 || height_ <= 0 || rgba_.empty();
}

int Image::width() const noexcept {
    return width_;
}

int Image::height() const noexcept {
    return height_;
}

const std::vector<std::uint8_t>& Image::pixels() const noexcept {
    return rgba_;
}

std::vector<std::uint8_t>& Image::pixels() noexcept {
    return rgba_;
}

Pixel Image::pixel(int x, int y) const {
    x = std::clamp(x, 0, width_ - 1);
    y = std::clamp(y, 0, height_ - 1);
    const auto index = static_cast<std::size_t>((y * width_ + x) * 4);
    return Pixel{rgba_[index], rgba_[index + 1], rgba_[index + 2], rgba_[index + 3]};
}

void Image::setPixel(int x, int y, Pixel pixel) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    const auto index = static_cast<std::size_t>((y * width_ + x) * 4);
    rgba_[index] = pixel.r;
    rgba_[index + 1] = pixel.g;
    rgba_[index + 2] = pixel.b;
    rgba_[index + 3] = pixel.a;
}

void Image::writePng(const std::filesystem::path& path) const {
    const int stride = width_ * 4;
    if (!stbi_write_png(path.string().c_str(), width_, height_, 4, rgba_.data(), stride)) {
        throw std::runtime_error("Failed to write PNG: " + path.string());
    }
}

void Image::writeJpeg(const std::filesystem::path& path, int quality) const {
    quality = std::clamp(quality, 1, 100);
    if (!stbi_write_jpg(path.string().c_str(), width_, height_, 4, rgba_.data(), quality)) {
        throw std::runtime_error("Failed to write JPEG: " + path.string());
    }
}

} // namespace grainrad
