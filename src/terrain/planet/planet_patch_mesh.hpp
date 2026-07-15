#pragma once

#include "renderer/objects/mesh_3d.hpp"
#include "terrain/planet/cube_sphere.hpp"
#include "terrain/planet/planet_patch.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace lve {

inline constexpr std::uint32_t PLANET_PATCH_QUADS = 32;
inline constexpr std::uint8_t MAX_FIXED_PLANET_PATCH_LEVEL = 3;

using PlanetHeightSampler = std::function<float(const wgen::PlanetSurfaceSample&)>;

struct PlanetPatchMeshData {
    wgen::PlanetPatchId id{};
    std::vector<Vertex3d> vertices{};
    std::vector<std::uint32_t> indices{};
};

float sampleCubeSphereBilinear(
    const wgen::CubeSphere<float>& source,
    const wgen::PlanetSurfaceSample& surface);

PlanetPatchMeshData buildPlanetPatchMesh(
    const wgen::PlanetPatchId& id,
    std::uint32_t quadCount,
    float planetRadius,
    const PlanetHeightSampler& heightSampler);

std::vector<PlanetPatchMeshData> buildFixedLevelPlanetPatchMeshes(
    const wgen::CubeSphere<float>& source,
    std::uint8_t level);

void appendCubeSphereMesh(
    const wgen::CubeSphere<float>& cubeSphere,
    std::vector<Vertex3d>& vertices,
    std::vector<std::uint32_t>& indices);

} // namespace lve
