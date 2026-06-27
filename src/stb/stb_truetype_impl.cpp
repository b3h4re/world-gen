#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "font_atlas.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace lve {

namespace {

std::vector<unsigned char> readFileBytes(const std::filesystem::path &path) {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) {
        throw std::runtime_error("failed to open font file");
    }

    const auto size = file.tellg();
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);

    return bytes;
}

} // namespace

const Glyph *FontAtlas::glyph(std::uint32_t codepoint) const {
    if (codepoint < firstCodepoint) {
        return nullptr;
    }

    const std::uint32_t index = codepoint - firstCodepoint;
    if (index >= glyphs.size()) {
        return nullptr;
    }

    return &glyphs[index];
}

FontAtlas bakeFontAtlas(const std::filesystem::path &fontPath, float pixelHeight) {
    constexpr int atlasWidth = 512;
    constexpr int atlasHeight = 512;
    constexpr int firstChar = 32;
    constexpr int charCount = 96;

    FontAtlas atlas{};
    atlas.width = atlasWidth;
    atlas.height = atlasHeight;
    atlas.pixelHeight = pixelHeight;
    atlas.firstCodepoint = firstChar;
    atlas.pixels.resize(atlasWidth * atlasHeight);

    auto fontBytes = readFileBytes(fontPath);
    std::vector<stbtt_bakedchar> bakedChars(charCount);

    const int result = stbtt_BakeFontBitmap(
        fontBytes.data(),
        0,
        pixelHeight,
        atlas.pixels.data(),
        atlasWidth,
        atlasHeight,
        firstChar,
        charCount,
        bakedChars.data()
    );

    if (result <= 0) {
        throw std::runtime_error("failed to bake font atlas");
    }

    atlas.glyphs.reserve(charCount);
    for (const auto &bakedChar : bakedChars) {
        atlas.glyphs.push_back({
            {
                static_cast<float>(bakedChar.x0) / static_cast<float>(atlasWidth),
                static_cast<float>(bakedChar.y0) / static_cast<float>(atlasHeight),
            },
            {
                static_cast<float>(bakedChar.x1) / static_cast<float>(atlasWidth),
                static_cast<float>(bakedChar.y1) / static_cast<float>(atlasHeight),
            },
            {
                static_cast<float>(bakedChar.x1 - bakedChar.x0),
                static_cast<float>(bakedChar.y1 - bakedChar.y0),
            },
            {
                bakedChar.xoff,
                bakedChar.yoff,
            },
            bakedChar.xadvance,
        });
    }

    return atlas;
}

FontFamily::FontFamily(const std::filesystem::path &fontPath, std::span<const float> pixelHeights) {
    if (pixelHeights.empty()) {
        throw std::runtime_error("font family requires at least one atlas size");
    }

    atlases_.reserve(pixelHeights.size());
    for (float pixelHeight : pixelHeights) {
        atlases_.push_back(bakeFontAtlas(fontPath, pixelHeight));
    }

    std::sort(atlases_.begin(), atlases_.end(), [](const FontAtlas &left, const FontAtlas &right) {
        return left.pixelHeight < right.pixelHeight;
    });
}

const FontAtlas &FontFamily::atlasForPixelHeight(float pixelHeight) const {
    if (atlases_.empty()) {
        throw std::runtime_error("font family has no atlases");
    }

    const FontAtlas *best = &atlases_.front();
    float bestDistance = std::numeric_limits<float>::max();
    for (const auto &atlas : atlases_) {
        const float distance = std::abs(atlas.pixelHeight - pixelHeight);
        if (distance < bestDistance) {
            best = &atlas;
            bestDistance = distance;
        }
    }

    return *best;
}

} // namespace lve
