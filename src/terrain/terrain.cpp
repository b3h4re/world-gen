#include "terrain.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace wgen {

HeightMap::HeightMap(std::size_t width, std::size_t height) : width_{width}, height_{height}, samples_(width * height) {
    if (width < 2 || height < 2) {
        throw std::invalid_argument("height map dimensions must be at least 2x2");
    }
}

HeightMap generatePreview(std::size_t width, std::size_t height, std::uint32_t seed) {
    HeightMap map{width, height};
    std::mt19937 random{seed};
    std::uniform_real_distribution<float> noise{-0.08F, 0.08F};

    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const float nx = static_cast<float>(x) / static_cast<float>(width - 1);
            const float ny = static_cast<float>(y) / static_cast<float>(height - 1);
            const float broad = 0.5F + 0.26F * std::sin(nx * 9.0F) * std::cos(ny * 7.0F);
            const float detail = 0.12F * std::sin((nx + ny) * 31.0F);
            map.at(x, y) = std::clamp(broad + detail + noise(random), 0.0F, 1.0F);
        }
    }

    return map;
}


}