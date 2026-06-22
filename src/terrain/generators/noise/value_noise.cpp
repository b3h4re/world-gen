#include "value_noise.hpp"

#include <random>
#include <algorithm>

namespace wgen {

    ValueNoiseGenerator::ValueNoiseGenerator() : ValueNoiseGenerator{std::random_device{}()} {}

    ValueNoiseGenerator::ValueNoiseGenerator(std::uint32_t seed) {
        setSeed(seed);
    }

    HeightMap<float> ValueNoiseGenerator::generateHeightMap(std::size_t width, std::size_t height) {
        HeightMap<float> map{width, height};
        std::mt19937 random{getSeed()};
        std::uniform_real_distribution<float> noise{-0.08F, 0.08F};

        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                map.at(x, y) = std::clamp(noise(random), 0.0F, 1.0F);
            }
        }

        return map;
    }

}
