#include "layered_sin_noise.hpp"

#include <random>
#include <algorithm>

namespace wgen {

    LayeredSinNoiseGenerator::LayeredSinNoiseGenerator() : LayeredSinNoiseGenerator{std::random_device{}()} {}

    LayeredSinNoiseGenerator::LayeredSinNoiseGenerator(std::uint32_t seed) {
        setSeed(seed);
    }

    HeightMap<float> LayeredSinNoiseGenerator::generateHeightMap(std::size_t width, std::size_t height) {
        HeightMap<float> map{width, height};
        std::mt19937 random{getSeed()};
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
