#include "layered_sin_noise.hpp"

#include <algorithm>
#include <random>

namespace wgen {
    namespace {

        constexpr std::size_t DEFAULT_TERRAIN_WIDTH = 96;
        constexpr float DEFAULT_TERRAIN_MAX_X = 95.0F;
        constexpr float DEFAULT_TERRAIN_MAX_Y = 63.0F;

        unsigned long long randomOffset(std::size_t x, std::size_t y) {
            return static_cast<unsigned long long>(y * DEFAULT_TERRAIN_WIDTH + x);
        }

    }

    LayeredSinNoiseGenerator::LayeredSinNoiseGenerator() : LayeredSinNoiseGenerator{std::random_device{}()} {}

    LayeredSinNoiseGenerator::LayeredSinNoiseGenerator(std::uint32_t seed) {
        setSeed(seed);
    }

    float LayeredSinNoiseGenerator::noise(std::size_t x, std::size_t y) const {
        std::mt19937 random{getSeed()};
        std::uniform_real_distribution<float> noise{-0.08F, 0.08F};
        random.discard(randomOffset(x, y));

        const float nx = static_cast<float>(x) / DEFAULT_TERRAIN_MAX_X;
        const float ny = static_cast<float>(y) / DEFAULT_TERRAIN_MAX_Y;
        const float broad = 0.5F + 0.26F * std::sin(nx * 9.0F) * std::cos(ny * 7.0F);
        const float detail = 0.12F * std::sin((nx + ny) * 31.0F);
        return std::clamp(broad + detail + noise(random), 0.0F, 1.0F);
    }

}
