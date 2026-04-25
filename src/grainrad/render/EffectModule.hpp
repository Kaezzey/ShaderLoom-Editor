#pragma once

#include "grainrad/Processing.hpp"

#include <cstdint>
#include <string>

namespace grainrad {

struct TextureRef {
    std::uint32_t id = 0;
    int width = 0;
    int height = 0;
};

struct FramebufferRef {
    std::uint32_t id = 0;
    int width = 0;
    int height = 0;
};

class EffectModule {
public:
    virtual ~EffectModule() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    virtual void drawSettingsUI() = 0;
    virtual void render(TextureRef inputTexture, FramebufferRef outputFramebuffer, const RenderContext& context) = 0;
};

} // namespace grainrad
