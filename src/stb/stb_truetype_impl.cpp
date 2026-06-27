#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <stdexcept>


struct FontAtlas {
    int width{};
    int height{};
    float pixelHeight{};
    std::vector<unsigned char> pixels;
    stbtt_bakedchar chars[96]; // ASCII 32..126
};


std::vector<unsigned char> readFileBytes(const std::filesystem::path& path) {
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


FontAtlas bakeFontAtlas(const std::filesystem::path& fontPath, float pixelHeight) {
    constexpr int atlasWidth = 512;
    constexpr int atlasHeight = 512;
    constexpr int firstChar = 32;
    constexpr int charCount = 96;

    FontAtlas atlas{};
    atlas.width = atlasWidth;
    atlas.height = atlasHeight;
    atlas.pixelHeight = pixelHeight;
    atlas.pixels.resize(atlasWidth * atlasHeight);

    auto fontBytes = readFileBytes(fontPath);

    const int result = stbtt_BakeFontBitmap(
        fontBytes.data(),
        0,
        pixelHeight,
        atlas.pixels.data(),
        atlasWidth,
        atlasHeight,
        firstChar,
        charCount,
        atlas.chars
    );

    if (result <= 0) {
        throw std::runtime_error("failed to bake font atlas");
    }

    return atlas;
}
