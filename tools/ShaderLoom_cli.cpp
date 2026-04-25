#include "ShaderLoom/Image.hpp"
#include "ShaderLoom/effects/AsciiEffect.hpp"
#include "ShaderLoom/effects/DitherEffect.hpp"
#include "ShaderLoom/effects/PixelSortEffect.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage() {
    std::cout << "Usage: ShaderLoom_cli <input> <output> <effect>\n"
              << "Effects: ascii, dither, pixelsort\n"
              << "ASCII supports .txt and .svg output.\n";
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void writeRaster(const ShaderLoom::Image& image, const std::filesystem::path& outputPath) {
    const std::string extension = lower(outputPath.extension().string());
    if (extension == ".jpg" || extension == ".jpeg") {
        image.writeJpeg(outputPath);
        return;
    }
    image.writePng(outputPath);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        printUsage();
        return argc == 1 ? 0 : 1;
    }

    try {
        const std::filesystem::path inputPath = argv[1];
        const std::filesystem::path outputPath = argv[2];
        const std::string effect = lower(argv[3]);

        const ShaderLoom::Image source = ShaderLoom::Image::load(inputPath);
        ShaderLoom::RenderContext context;

        if (effect == "ascii") {
            ShaderLoom::AsciiEffect ascii;
            ShaderLoom::AsciiSettings settings;
            const ShaderLoom::AsciiResult result = ascii.generate(source, settings, context);
            const std::string extension = lower(outputPath.extension().string());
            if (extension == ".svg") {
                ascii.writeSvg(result, outputPath, source.width(), source.height());
            } else {
                ascii.writeText(result, outputPath);
            }
            return 0;
        }

        if (effect == "dither") {
            ShaderLoom::DitherEffect dither;
            ShaderLoom::DitherSettings settings;
            settings.intensity = 1.0F;
            writeRaster(dither.apply(source, settings, context), outputPath);
            return 0;
        }

        if (effect == "pixelsort") {
            ShaderLoom::PixelSortEffect pixelSort;
            ShaderLoom::PixelSortSettings settings;
            writeRaster(pixelSort.apply(source, settings, context), outputPath);
            return 0;
        }

        std::cerr << "Unknown effect: " << effect << '\n';
        printUsage();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "ShaderLoom_cli failed: " << error.what() << '\n';
        return 1;
    }
}
