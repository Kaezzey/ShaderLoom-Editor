#include "ShaderLoom/Image.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace ShaderLoom {
namespace {

std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension;
}

std::vector<std::uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open image: " + path.string());
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size <= 0) {
        throw std::runtime_error("Image file is empty: " + path.string());
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!input) {
        throw std::runtime_error("Failed to read image: " + path.string());
    }
    return bytes;
}

} // namespace

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
    std::vector<ImageFrame> frames = loadImageFrames(path);
    return std::move(frames.front().image);
}

std::vector<ImageFrame> loadImageFrames(const std::filesystem::path& path) {
    if (lowerExtension(path) == ".gif") {
        const std::vector<std::uint8_t> bytes = readFileBytes(path);
        int width = 0;
        int height = 0;
        int frameCount = 0;
        int channels = 0;
        int* delays = nullptr;
        stbi_uc* loaded = stbi_load_gif_from_memory(
            bytes.data(),
            static_cast<int>(bytes.size()),
            &delays,
            &width,
            &height,
            &frameCount,
            &channels,
            4
        );
        if (!loaded) {
            throw std::runtime_error("Failed to load GIF: " + path.string());
        }

        std::vector<ImageFrame> frames;
        frames.reserve(static_cast<std::size_t>(std::max(frameCount, 1)));
        const auto frameBytes = static_cast<std::size_t>(width * height * 4);
        for (int frame = 0; frame < frameCount; ++frame) {
            const stbi_uc* begin = loaded + (static_cast<std::size_t>(frame) * frameBytes);
            std::vector<std::uint8_t> rgba(begin, begin + frameBytes);
            const int delay = delays != nullptr && delays[frame] > 0 ? delays[frame] : 100;
            frames.push_back(ImageFrame{Image(width, height, std::move(rgba)), delay});
        }

        stbi_image_free(loaded);
        if (delays != nullptr) {
            stbi_image_free(delays);
        }
        if (frames.empty()) {
            throw std::runtime_error("GIF contained no frames: " + path.string());
        }
        return frames;
    }

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
    return {ImageFrame{Image(width, height, std::move(rgba)), 100}};
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

} // namespace ShaderLoom
