#pragma once

#include "grainrad/Image.hpp"
#include "grainrad/Processing.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace grainrad {

struct AsciiSettings {
    float scale = 1.0F;
    float spacing = 0.3F;
    int outputWidth = 0;
    std::string characterSet = "DETAILED";
};

struct AsciiResult {
    int columns = 0;
    int rows = 0;
    std::vector<std::string> lines;
};

class AsciiEffect {
public:
    [[nodiscard]] AsciiResult generate(const Image& source, const AsciiSettings& settings, const RenderContext& context) const;
    void writeText(const AsciiResult& result, const std::filesystem::path& path) const;
    void writeSvg(const AsciiResult& result, const std::filesystem::path& path, int sourceWidth, int sourceHeight) const;

private:
    [[nodiscard]] std::string glyphsFor(const std::string& name) const;
};

} // namespace grainrad
