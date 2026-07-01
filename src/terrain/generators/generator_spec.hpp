#pragma once

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
    std::size_t gridWidth{100};
    std::size_t gridHeight{100};
    std::size_t dotsPerCell{100};
};

using GeneratorConfig = std::variant<ValueNoiseGeneratorSpec, PerlinNoiseGeneratorSpec>;

struct GeneratorSpec {
    GeneratorKind kind{GeneratorKind::ValueNoise};
    GeneratorConfig config{ValueNoiseGeneratorSpec{}};
};

using GeneratorPipelineSpec = std::vector<GeneratorSpec>;

} // namespace wgen
