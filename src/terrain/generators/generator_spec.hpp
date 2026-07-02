#pragma once

#include "terrain/terrain_compute_method.hpp"

#include <cmath>
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

constexpr bool generatorSupportsOctaves(GeneratorKind kind) {
    switch (kind) {
        case GeneratorKind::ValueNoise:
        case GeneratorKind::PerlinNoise:
        case GeneratorKind::WorleyNoise:
        case GeneratorKind::SimplexNoise:
            return true;
    }

    return false;
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

struct GeneratorOctaveSettings {
    std::size_t numOctave{0};
    float lacunarity{1.0F};
    float persistance{1.0F};
};

inline float octaveFrequency(const GeneratorOctaveSettings& settings) {
    return std::pow(settings.lacunarity, static_cast<float>(settings.numOctave));
}

inline float octaveAmplitude(const GeneratorOctaveSettings& settings) {
    return std::pow(settings.persistance, static_cast<float>(settings.numOctave));
}

struct GeneratorSpec {
    GeneratorKind kind{GeneratorKind::ValueNoise};
    GeneratorConfig config{ValueNoiseGeneratorSpec{}};
    float scale{1.0F};
    TerrainComputeMethod computeMethod{defaultComputeMethodForGenerator(kind)};
    GeneratorOctaveSettings octaveSettings{};
};

inline float generatorOctaveFrequency(const GeneratorSpec& spec) {
    if (!generatorSupportsOctaves(spec.kind)) {
        return 1.0F;
    }

    return octaveFrequency(spec.octaveSettings);
}

inline float generatorOctaveAmplitude(const GeneratorSpec& spec) {
    if (!generatorSupportsOctaves(spec.kind)) {
        return 1.0F;
    }

    return octaveAmplitude(spec.octaveSettings);
}

using GeneratorPipelineSpec = std::vector<GeneratorSpec>;

} // namespace wgen
