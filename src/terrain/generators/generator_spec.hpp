#pragma once

#include "terrain/terrain_compute_method.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace wgen {

enum class GeneratorKind {
    ValueNoise,
    PerlinNoise,
};

struct ValueNoiseGeneratorSpec {};

struct PerlinNoiseGeneratorSpec {
    std::size_t dotsPerCell{100};
};

using GeneratorConfig = std::variant<ValueNoiseGeneratorSpec, PerlinNoiseGeneratorSpec>;

struct GeneratorSpec {
    GeneratorKind kind{GeneratorKind::ValueNoise};
    GeneratorConfig config{ValueNoiseGeneratorSpec{}};
    float scale{1.0F};
    TerrainComputeMethod computeMethod{TerrainComputeMethod::Cpu};
};

using GeneratorPipelineSpec = std::vector<GeneratorSpec>;

} // namespace wgen
