#pragma once

#include "terrain/generators/2d/generator_spec.hpp"
#include "terrain/terrain_compute_method.hpp"

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace wgen {

enum class Generator3dKind {
    PerlinNoise
};

constexpr bool generator3dSupportsVulkanCompute(Generator3dKind) {
    return true;
}

constexpr TerrainComputeMethod defaultComputeMethodForGenerator3d(Generator3dKind kind) {
    return generator3dSupportsVulkanCompute(kind)
        ? TerrainComputeMethod::VulkanCompute
        : TerrainComputeMethod::Cpu;
}

constexpr bool generator3dSupportsOctaves(Generator3dKind) {
    return true;
}

constexpr float generator3dMaximumAbsoluteNoise(Generator3dKind kind) {
    switch (kind) {
        case Generator3dKind::PerlinNoise:
            return 1.7320508F;
    }
    return 0.0F;
}

struct PerlinNoise3dGeneratorSpec {
    float cellSize{0.5F};
};

using Generator3dConfig = std::variant<PerlinNoise3dGeneratorSpec>;

struct Generator3dTerrainDetailSpec {
    // The contributor fades in during the preceding continuous detail level
    // and is fully visible at this level. When absent, firstVisibleLod is used
    // as a compatibility input for existing pipeline specifications.
    std::optional<std::uint8_t> firstFullyVisibleLevel{};
};

struct Generator3dSpec {
    Generator3dKind kind{Generator3dKind::PerlinNoise};
    Generator3dConfig config{PerlinNoise3dGeneratorSpec{}};
    float scale{1.0F};
    TerrainComputeMethod computeMethod{defaultComputeMethodForGenerator3d(kind)};
    GeneratorOctaveSettings octaveSettings{};
    // Compatibility setting retained for existing callers and saved pipeline
    // data. New code should use terrainDetail.firstFullyVisibleLevel.
    std::uint8_t firstVisibleLod{0};
    Generator3dTerrainDetailSpec terrainDetail{};
};

inline std::uint8_t generator3dFirstFullyVisibleDetail(const Generator3dSpec& spec) {
    return spec.terrainDetail.firstFullyVisibleLevel.value_or(spec.firstVisibleLod);
}

inline float generator3dOctaveFrequency(const Generator3dSpec& spec) {
    if (!generator3dSupportsOctaves(spec.kind)) {
        return 1.0F;
    }

    return octaveFrequency(spec.octaveSettings);
}

inline float generator3dOctaveAmplitude(const Generator3dSpec& spec) {
    if (!generator3dSupportsOctaves(spec.kind)) {
        return 1.0F;
    }

    return octaveAmplitude(spec.octaveSettings);
}

using Generator3dPipelineSpec = std::vector<Generator3dSpec>;

} // namespace wgen
