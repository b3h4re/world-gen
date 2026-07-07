#pragma once

#include "terrain/terrain_compute_method.hpp"

#include <variant>
#include <vector>

namespace wgen {

enum class Generator3dKind {
    PerlinNoise
};

constexpr bool generator3dSupportsVulkanCompute(Generator3dKind) {
    return false;
}

constexpr TerrainComputeMethod defaultComputeMethodForGenerator3d(Generator3dKind kind) {
    return generator3dSupportsVulkanCompute(kind)
        ? TerrainComputeMethod::VulkanCompute
        : TerrainComputeMethod::Cpu;
}

constexpr bool generator3dSupportsOctaves(Generator3dKind) {
    return false;
}

struct PerlinNoise3dGeneratorSpec {
    float cellSize{0.5F};
};

using Generator3dConfig = std::variant<PerlinNoise3dGeneratorSpec>;

struct Generator3dSpec {
    Generator3dKind kind{Generator3dKind::PerlinNoise};
    Generator3dConfig config{PerlinNoise3dGeneratorSpec{}};
    float scale{1.0F};
    TerrainComputeMethod computeMethod{defaultComputeMethodForGenerator3d(kind)};
};

using Generator3dPipelineSpec = std::vector<Generator3dSpec>;

} // namespace wgen
