#pragma once

#include <stb_truetype.h>

#include <filesystem>
#include <vector>

namespace lve {

struct FontAtlas {
    int width{};
    int height{};
    float pixelHeight{};
    std::vector<unsigned char> pixels;
    stbtt_bakedchar chars[96]; // ASCII 32..126
};

FontAtlas bakeFontAtlas(const std::filesystem::path &fontPath, float pixelHeight);

} // namespace lve
