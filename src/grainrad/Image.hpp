#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace grainrad {

struct Pixel {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

class Image {
public:
    Image() = default;
    Image(int width, int height);
    Image(int width, int height, std::vector<std::uint8_t> rgba);

    static Image load(const std::filesystem::path& path);

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const noexcept;
    [[nodiscard]] std::vector<std::uint8_t>& pixels() noexcept;

    [[nodiscard]] Pixel pixel(int x, int y) const;
    void setPixel(int x, int y, Pixel pixel);

    void writePng(const std::filesystem::path& path) const;
    void writeJpeg(const std::filesystem::path& path, int quality = 92) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> rgba_;
};

} // namespace grainrad
