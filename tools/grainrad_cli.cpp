#include "grainrad/Image.hpp"
#include "grainrad/effects/AsciiEffect.hpp"
#include "grainrad/effects/DitherEffect.hpp"
#include "grainrad/effects/PixelSortEffect.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage() {
    std::cout << "Usage: grainrad_cli <input> <output> <effect>\n"
              << "Effects: ascii, dither, pixelsort\n"
              << "ASCII supports .txt and .svg output.\n";
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void writeRaster(const grainrad::Image& image, const std::filesystem::path& outputPath) {
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

        const grainrad::Image source = grainrad::Image::load(inputPath);
        grainrad::RenderContext context;

        if (effect == "ascii") {
            grainrad::AsciiEffect ascii;
            grainrad::AsciiSettings settings;
            const grainrad::AsciiResult result = ascii.generate(source, settings, context);
            const std::string extension = lower(outputPath.extension().string());
            if (extension == ".svg") {
                ascii.writeSvg(result, outputPath, source.width(), source.height());
            } else {
                ascii.writeText(result, outputPath);
            }
            return 0;
        }

        if (effect == "dither") {
            grainrad::DitherEffect dither;
            grainrad::DitherSettings settings;
            settings.intensity = 1.0F;
            writeRaster(dither.apply(source, settings, context), outputPath);
            return 0;
        }

        if (effect == "pixelsort") {
            grainrad::PixelSortEffect pixelSort;
            grainrad::PixelSortSettings settings;
            writeRaster(pixelSort.apply(source, settings, context), outputPath);
            return 0;
        }

        std::cerr << "Unknown effect: " << effect << '\n';
        printUsage();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "grainrad_cli failed: " << error.what() << '\n';
        return 1;
    }
}
