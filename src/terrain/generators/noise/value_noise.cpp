#include "value_noise.hpp"

#include "terrain/utils/hash_random.hpp"

#include <algorithm>
#include <random>

namespace wgen {

    ValueNoiseGenerator::ValueNoiseGenerator() : ValueNoiseGenerator{std::random_device{}()} {}

    ValueNoiseGenerator::ValueNoiseGenerator(SeedType seed) {
        setSeed(seed);
    }

    float ValueNoiseGenerator::noise(std::size_t x, std::size_t y) const {
        HashUniformRealDistribution<float> noise{-0.08F, 0.08F};
        return std::clamp(noise(hashValues(getSeed(), x, y)), 0.0F, 1.0F);
    }

    GeneratorCapabilities ValueNoiseGenerator::capabilities() const {
        return {
            .cpu = true,
            .vulkanCompute = true,
            .octaves = true,
        };
    }

    std::string ValueNoiseGenerator::compShader() const {
        return "value_noise";
    }

    std::size_t ValueNoiseGenerator::specSize() const {
        return sizeof(ValueNoiseComputeSpec);
    }

}
