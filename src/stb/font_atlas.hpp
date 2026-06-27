#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace lve {

struct Glyph {
    glm::vec2 uvMin;
    glm::vec2 uvMax;
    glm::vec2 size;
    glm::vec2 bearing;
    float advance;
};

struct FontAtlas {
    int width{};
    int height{};
    float pixelHeight{};
    std::uint32_t firstCodepoint{32};
    std::vector<unsigned char> pixels;
    std::vector<Glyph> glyphs;

    const Glyph *glyph(std::uint32_t codepoint) const;
};

FontAtlas bakeFontAtlas(const std::filesystem::path &fontPath, float pixelHeight);

} // namespace lve
