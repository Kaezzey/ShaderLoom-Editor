#include "ShaderLoom/effects/AsciiEffect.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace ShaderLoom {
namespace {

std::string utf8ForCodepoint(std::uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        return std::string(1, static_cast<char>(codepoint));
    }
    if (codepoint <= 0x7FFU) {
        return std::string{
            static_cast<char>(0xC0U | ((codepoint >> 6U) & 0x1FU)),
            static_cast<char>(0x80U | (codepoint & 0x3FU))
        };
    }
    if (codepoint <= 0xFFFFU) {
        return std::string{
            static_cast<char>(0xE0U | ((codepoint >> 12U) & 0x0FU)),
            static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)),
            static_cast<char>(0x80U | (codepoint & 0x3FU))
        };
    }
    return std::string{
        static_cast<char>(0xF0U | ((codepoint >> 18U) & 0x07U)),
        static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)),
        static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)),
        static_cast<char>(0x80U | (codepoint & 0x3FU))
    };
}

std::string escapeSvg(char glyph) {
    switch (glyph) {
    case '&':
        return "&amp;";
    case '<':
        return "&lt;";
    case '>':
        return "&gt;";
    case '"':
        return "&quot;";
    default:
        return std::string(1, glyph);
    }
}

std::string escapeSvg(const std::string& glyph) {
    std::string escaped;
    escaped.reserve(glyph.size());
    for (char c : glyph) {
        escaped += escapeSvg(c);
    }
    return escaped;
}

} // namespace

AsciiResult AsciiEffect::generate(const Image& source, const AsciiSettings& settings, const RenderContext& context) const {
    const std::vector<AsciiGlyphToken> glyphs = glyphsFor(settings.characterSet);
    const Image processedSource = applyProcessing(source, context);
    const int maxColumns = std::max(12, std::min(source.width(), 2048));
    const int columns = settings.outputWidth > 0
        ? std::clamp(settings.outputWidth, 12, maxColumns)
        : std::clamp(
            static_cast<int>(std::round(source.width() / std::max(4.0F, 8.0F / std::max(settings.scale, 0.1F)))),
            12,
            std::min(maxColumns, 260)
        );

    const float aspect = static_cast<float>(source.height()) / static_cast<float>(std::max(source.width(), 1));
    const float characterAspect = 0.48F + std::clamp(settings.spacing, 0.0F, 2.0F);
    const int rows = std::max(1, static_cast<int>(std::round(columns * aspect * characterAspect)));

    AsciiResult result;
    result.columns = columns;
    result.rows = rows;
    result.lines.reserve(static_cast<std::size_t>(rows));
    result.glyphCodes.reserve(static_cast<std::size_t>(rows));

    for (int row = 0; row < rows; ++row) {
        std::string line;
        std::vector<std::uint32_t> lineCodes;
        lineCodes.reserve(static_cast<std::size_t>(columns));
        for (int col = 0; col < columns; ++col) {
            const float u = (static_cast<float>(col) + 0.5F) / static_cast<float>(columns);
            const float v = (static_cast<float>(row) + 0.5F) / static_cast<float>(rows);
            const int x = std::clamp(static_cast<int>(u * source.width()), 0, source.width() - 1);
            const int y = std::clamp(static_cast<int>(v * source.height()), 0, source.height() - 1);
            const float luma = luminance(processedSource.pixel(x, y));
            auto glyphIndex = static_cast<std::size_t>(std::round(luma * static_cast<float>(glyphs.size() - 1)));
            if ((settings.characterSet == "BINARY" || settings.characterSet == "DETAILED") && glyphIndex == 0 && glyphs.size() > 1) {
                glyphIndex = 1;
            }
            const AsciiGlyphToken& glyph = glyphs[std::clamp(glyphIndex, std::size_t{0}, glyphs.size() - 1)];
            line += glyph.utf8;
            lineCodes.push_back(glyph.codepoint);
        }
        result.lines.push_back(std::move(line));
        result.glyphCodes.push_back(std::move(lineCodes));
    }

    return result;
}

void AsciiEffect::writeText(const AsciiResult& result, const std::filesystem::path& path) const {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to open ASCII text export: " + path.string());
    }

    for (const std::string& line : result.lines) {
        output << line << '\n';
    }
}

void AsciiEffect::writeSvg(const AsciiResult& result, const std::filesystem::path& path, int sourceWidth, int sourceHeight) const {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to open ASCII SVG export: " + path.string());
    }

    const float fontSize = std::max(1.0F, static_cast<float>(sourceWidth) / static_cast<float>(std::max(result.columns, 1)));
    const float lineHeight = static_cast<float>(sourceHeight) / static_cast<float>(std::max(result.rows, 1));

    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << sourceWidth
           << "\" height=\"" << sourceHeight << "\" viewBox=\"0 0 " << sourceWidth << ' ' << sourceHeight << "\">\n";
    output << "<g fill=\"#fff\" font-family=\"Consolas, monospace\" font-size=\"" << fontSize << "\">\n";
    for (int row = 0; row < result.rows; ++row) {
        const float y = (static_cast<float>(row) + 1.0F) * lineHeight;
        for (int col = 0; col < result.columns; ++col) {
            const float x = static_cast<float>(col) * fontSize;
            output << "<text x=\"" << x << "\" y=\"" << y << "\">"
                   << escapeSvg(utf8ForCodepoint(result.glyphCodes[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)]))
                   << "</text>\n";
        }
    }
    output << "</g>\n</svg>\n";
}

std::vector<AsciiGlyphToken> AsciiEffect::glyphsFor(const std::string& name) const {
    auto asciiGlyphs = [](const char* ascii) {
        std::vector<AsciiGlyphToken> glyphs;
        for (const char* c = ascii; *c != '\0'; ++c) {
            glyphs.push_back({static_cast<std::uint32_t>(static_cast<unsigned char>(*c)), std::string(1, *c)});
        }
        return glyphs;
    };

    if (name == "STANDARD") {
        return asciiGlyphs(" .:-=+*#%@");
    }
    if (name == "BLOCKS") {
        return {
            {32U, " "},
            {0x2591U, "\xE2\x96\x91"},
            {0x2592U, "\xE2\x96\x92"},
            {0x2593U, "\xE2\x96\x93"},
            {0x2588U, "\xE2\x96\x88"}
        };
    }
    if (name == "BINARY") {
        return asciiGlyphs(" 01");
    }
    if (name == "DETAILED") {
        return asciiGlyphs(" .^\",:;Il!i><~+-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$");
    }
    if (name == "MINIMAL") {
        return asciiGlyphs(" .:-=+*#%@");
    }
    if (name == "ALPHABETIC") {
        return asciiGlyphs(" .,:ilcvunxrjftLCJUYXZO0QdbpqwmhaoMW");
    }
    if (name == "NUMERIC") {
        return asciiGlyphs(" 1234567890");
    }
    if (name == "MATH") {
        return asciiGlyphs(" .-+=*/%#@");
    }
    if (name == "SYMBOLS") {
        return asciiGlyphs(" .,:;!<>?/|\\{}[]()#$%@");
    }
    return asciiGlyphs(" .:-=+*#%@");
}

} // namespace ShaderLoom
