#include "value_noise.hpp"

#include <algorithm>
#include <random>

namespace wgen {
    namespace {

        constexpr std::size_t DEFAULT_TERRAIN_WIDTH = 96;

        unsigned long long randomOffset(std::size_t x, std::size_t y) {
            return static_cast<unsigned long long>(y * DEFAULT_TERRAIN_WIDTH + x);
        }

    }

    ValueNoiseGenerator::ValueNoiseGenerator() : ValueNoiseGenerator{std::random_device{}()} {}

    ValueNoiseGenerator::ValueNoiseGenerator(std::uint32_t seed) {
        setSeed(seed);
    }

    float ValueNoiseGenerator::noise(std::size_t x, std::size_t y) const {
        std::mt19937 random{getSeed()};
        std::uniform_real_distribution<float> noise{-0.08F, 0.08F};
        random.discard(randomOffset(x, y));
        return std::clamp(noise(random), 0.0F, 1.0F);
    }

}
