#pragma once

#include "renderer/objects/mesh_3d.hpp"
#include "terrain/planet/cube_sphere.hpp"
#include "terrain/planet/planet_patch.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace lve {

inline constexpr std::uint32_t PLANET_PATCH_QUADS = 32;
inline constexpr std::uint8_t MAX_FIXED_PLANET_PATCH_LEVEL = 3;

using PlanetHeightSampler = std::function<float(const wgen::PlanetSurfaceSample&)>;

struct PlanetPatchVersion {
    std::uint64_t terrainEpoch{};
    std::uint64_t dependencyRevision{};
    std::uint64_t requestRevision{};

    bool operator==(const PlanetPatchVersion&) const = default;
};

struct PlanetPatchMeshRequest {
    wgen::PlanetPatchId id{};
    PlanetPatchVersion version{};
};

struct PlanetPatchMeshData {
    wgen::PlanetPatchId id{};
    PlanetPatchVersion version{};
    std::vector<Vertex3d> vertices{};
    std::vector<std::uint32_t> indices{};
};

struct PlanetPatchRemoval {
    wgen::PlanetPatchId id{};
    std::uint64_t terrainEpoch{};
    std::uint64_t requestRevision{};
};

struct PlanetPatchMeshBatch {
    std::uint64_t terrainEpoch{};
    std::uint64_t requestRevision{};
    std::vector<PlanetPatchMeshData> upserts{};
    std::vector<PlanetPatchRemoval> removals{};
};

enum class PlanetPatchBatchEpochAction : std::uint8_t {
    Discard,
    Apply,
    Replace,
};

float sampleCubeSphereBilinear(
    const wgen::CubeSphere<float>& source,
    const wgen::PlanetSurfaceSample& surface);

PlanetPatchMeshData buildPlanetPatchMesh(
    const wgen::PlanetPatchId& id,
    std::uint32_t quadCount,
    float planetRadius,
    const PlanetHeightSampler& heightSampler);

PlanetPatchMeshData buildRequestedPlanetPatchMesh(
    const PlanetPatchMeshRequest& request,
    std::uint32_t quadCount,
    float planetRadius,
    const PlanetHeightSampler& heightSampler);

std::vector<PlanetPatchMeshData> buildFixedLevelPlanetPatchMeshes(
    const wgen::CubeSphere<float>& source,
    std::uint8_t level);

std::vector<PlanetPatchMeshData> buildFixedLevelPlanetPatchMeshes(
    float planetRadius,
    std::uint8_t level,
    const PlanetHeightSampler& heightSampler);

std::vector<PlanetPatchMeshData> buildFixedLevelPlanetPatchMeshes(
    float planetRadius,
    std::uint8_t level,
    const PlanetPatchVersion& version,
    const PlanetHeightSampler& heightSampler);

std::vector<wgen::PlanetPatchId> fixedLevelPlanetPatchIds(std::uint8_t level);

std::vector<wgen::PlanetPatchId> planetPatchIdDifference(
    std::span<const wgen::PlanetPatchId> previousIds,
    std::span<const wgen::PlanetPatchId> desiredIds);

PlanetPatchMeshBatch makePlanetPatchMeshBatch(
    std::vector<PlanetPatchMeshData> upserts,
    std::span<const wgen::PlanetPatchId> previousIds,
    std::uint64_t terrainEpoch,
    std::uint64_t requestRevision);

void validatePlanetPatchMeshBatch(const PlanetPatchMeshBatch& batch);

bool isCurrentPlanetPatchMeshBatch(
    const PlanetPatchMeshBatch& batch,
    std::uint64_t desiredRequestRevision) noexcept;

bool discardStalePlanetPatchMeshBatch(
    std::optional<PlanetPatchMeshBatch>& batch,
    std::uint64_t desiredRequestRevision) noexcept;

PlanetPatchBatchEpochAction planetPatchBatchEpochAction(
    std::uint64_t batchTerrainEpoch,
    std::uint64_t activeTerrainEpoch) noexcept;

bool shouldApplyPlanetPatchUpsert(
    const PlanetPatchVersion& incoming,
    const PlanetPatchVersion& resident) noexcept;

bool shouldApplyPlanetPatchRemoval(
    const PlanetPatchRemoval& removal,
    const PlanetPatchVersion& resident) noexcept;

void appendCubeSphereMesh(
    const wgen::CubeSphere<float>& cubeSphere,
    std::vector<Vertex3d>& vertices,
    std::vector<std::uint32_t>& indices);

} // namespace lve
