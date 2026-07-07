#include "layered_sin_noise.hpp"

#include "terrain/utils/hash_random.hpp"

#include <algorithm>
#include <random>

namespace wgen {
    namespace {

        constexpr float DEFAULT_TERRAIN_MAX_X = 95.0F;
        constexpr float DEFAULT_TERRAIN_MAX_Y = 63.0F;

    }

    LayeredSinNoiseGenerator::LayeredSinNoiseGenerator() : LayeredSinNoiseGenerator{std::random_device{}()} {}

    LayeredSinNoiseGenerator::LayeredSinNoiseGenerator(SeedType seed) {
        setSeed(seed);
    }

    float LayeredSinNoiseGenerator::noise(std::size_t x, std::size_t y) const {
        HashUniformRealDistribution<float> noise{-0.08F, 0.08F};

        const float nx = static_cast<float>(x) / DEFAULT_TERRAIN_MAX_X;
        const float ny = static_cast<float>(y) / DEFAULT_TERRAIN_MAX_Y;
        const float broad = 0.5F + 0.26F * std::sin(nx * 9.0F) * std::cos(ny * 7.0F);
        const float detail = 0.12F * std::sin((nx + ny) * 31.0F);
        return std::clamp(broad + detail + noise(hashValues(getSeed(), x, y)), 0.0F, 1.0F);
    }

}
