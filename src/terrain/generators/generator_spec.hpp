#pragma once

#include "terrain/terrain_compute_method.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace wgen {

enum class GeneratorKind {
    ValueNoise,
    PerlinNoise,
    WorleyNoise,
    SimplexNoise
};

constexpr bool generatorSupportsVulkanCompute(GeneratorKind kind) {
    switch (kind) {
        case GeneratorKind::ValueNoise:
        case GeneratorKind::PerlinNoise:
        case GeneratorKind::WorleyNoise:
        case GeneratorKind::SimplexNoise:
            return true;
    }

    return false;
}

constexpr TerrainComputeMethod defaultComputeMethodForGenerator(GeneratorKind kind) {
    return generatorSupportsVulkanCompute(kind)
        ? TerrainComputeMethod::VulkanCompute
        : TerrainComputeMethod::Cpu;
}

struct ValueNoiseGeneratorSpec {};

struct PerlinNoiseGeneratorSpec {
    std::size_t dotsPerCell{100};
};

struct SimplexNoiseGeneratorSpec {
    std::size_t dotsPerCell{100};
};


struct WorleyNoiseGeneratorSpec {
    std::size_t dotsPerCell{100};
    std::size_t numPoints{1};
    float p{2.0F};
};

using GeneratorConfig = std::variant<
    ValueNoiseGeneratorSpec,
    PerlinNoiseGeneratorSpec,
    WorleyNoiseGeneratorSpec,
    SimplexNoiseGeneratorSpec
>;

struct GeneratorSpec {
    GeneratorKind kind{GeneratorKind::ValueNoise};
    GeneratorConfig config{ValueNoiseGeneratorSpec{}};
    float scale{1.0F};
    TerrainComputeMethod computeMethod{defaultComputeMethodForGenerator(kind)};
};

using GeneratorPipelineSpec = std::vector<GeneratorSpec>;

} // namespace wgen
