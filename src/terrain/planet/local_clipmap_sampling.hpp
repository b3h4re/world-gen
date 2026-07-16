#pragma once

#include "terrain/planet/local_clipmap_topology.hpp"
#include "terrain/planet/local_planet_frame.hpp"
#include "terrain/planet/terrain_field.hpp"

#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace wgen {

struct LocalClipmapSamplingConfig {
    // The standalone debug path begins tangent-flat. Later hybrid stages can
    // raise this weight or force only the handoff ring to the curved surface.
    double curvatureWeight{};
    bool forceOutermostRingCurved{};
};

struct LocalClipmapSurfaceSample {
    LocalClipmapGridCoordinate gridCoordinate{};
    glm::dvec2 localPositionMeters{};
    glm::dvec3 globalDirection{};
    TerrainDetailLevel terrainDetail{0.0};
    float heightMeters{};
    glm::dvec3 curvedCameraRelativePositionMeters{};
    glm::dvec3 tangentFlatCameraRelativePositionMeters{};
    glm::dvec3 blendedCameraRelativePositionMeters{};
    double curvatureWeight{};
};

void validateLocalClipmapSamplingConfig(
    const LocalClipmapSamplingConfig& config);

double localClipmapCurvatureWeight(
    const LocalClipmapSamplingConfig& config,
    std::uint32_t level,
    std::uint32_t levelCount);

LocalClipmapSurfaceSample sampleLocalClipmapPoint(
    const LocalPlanetFrame& frame,
    const TerrainFieldSnapshot& terrainField,
    const LocalClipmapConfig& clipmapConfig,
    const LocalClipmapCacheOrigin& cacheOrigin,
    const LocalClipmapGridCoordinate& sampleCoordinate,
    const glm::dvec3& cameraGlobalPositionMeters,
    const LocalClipmapSamplingConfig& samplingConfig = {});

// Samples the complete batch through one immutable TerrainField snapshot and
// preserves input order for deterministic cache-region writes.
std::vector<LocalClipmapSurfaceSample> sampleLocalClipmapPoints(
    const LocalPlanetFrame& frame,
    const TerrainFieldSnapshot& terrainField,
    const LocalClipmapConfig& clipmapConfig,
    const LocalClipmapCacheOrigin& cacheOrigin,
    std::span<const LocalClipmapGridCoordinate> sampleCoordinates,
    const glm::dvec3& cameraGlobalPositionMeters,
    const LocalClipmapSamplingConfig& samplingConfig = {});

} // namespace wgen
