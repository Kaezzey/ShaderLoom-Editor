#pragma once

#include "ShaderLoom/Image.hpp"
#include "ShaderLoom/Processing.hpp"

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace ShaderLoom {

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
    std::vector<std::vector<std::uint32_t>> glyphCodes;
};

struct AsciiGlyphToken {
    std::uint32_t codepoint = 32;
    std::string utf8 = " ";
};

class AsciiEffect {
public:
    [[nodiscard]] AsciiResult generate(const Image& source, const AsciiSettings& settings, const RenderContext& context) const;
    void writeText(const AsciiResult& result, const std::filesystem::path& path) const;
    void writeSvg(const AsciiResult& result, const std::filesystem::path& path, int sourceWidth, int sourceHeight) const;

private:
    [[nodiscard]] std::vector<AsciiGlyphToken> glyphsFor(const std::string& name) const;
};

} // namespace ShaderLoom
