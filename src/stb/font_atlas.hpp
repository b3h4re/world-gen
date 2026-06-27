#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
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

class FontFamily {
public:
    FontFamily() = default;
    FontFamily(const std::filesystem::path &fontPath, std::span<const float> pixelHeights);

    const FontAtlas &atlasForPixelHeight(float pixelHeight) const;
    const std::vector<FontAtlas> &atlases() const { return atlases_; }
    bool empty() const { return atlases_.empty(); }

private:
    std::vector<FontAtlas> atlases_{};
};

} // namespace lve
