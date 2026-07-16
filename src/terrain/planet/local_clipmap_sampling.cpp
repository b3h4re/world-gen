#include "terrain/planet/local_clipmap_sampling.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace wgen {
namespace {

bool isFinite(const glm::dvec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

void validateSamplingContext(
        const LocalPlanetFrame& frame,
        const TerrainFieldSnapshot& terrainField,
        const LocalClipmapConfig& clipmapConfig,
        const LocalClipmapCacheOrigin& cacheOrigin,
        const glm::dvec3& cameraGlobalPositionMeters,
        const LocalClipmapSamplingConfig& samplingConfig) {
    validateLocalPlanetFrame(frame);
    validateLocalClipmapConfig(clipmapConfig);
    validateLocalClipmapSamplingConfig(samplingConfig);
    if (terrainField == nullptr) {
        throw std::invalid_argument{
            "local clipmap sampling requires a terrain field snapshot"};
    }
    if (!isFinite(cameraGlobalPositionMeters)) {
        throw std::invalid_argument{
            "local clipmap camera position must be finite"};
    }
    if (cacheOrigin.level >= clipmapConfig.levelCount) {
        throw std::out_of_range{
            "local clipmap sampling level is outside the configured range"};
    }
    if (cacheOrigin.frameRevision != frame.revision) {
        throw std::invalid_argument{
            "local clipmap cache origin belongs to a different local frame"};
    }

    const float frameRadiusAtFieldPrecision =
        static_cast<float>(frame.planetRadiusMeters);
    if (!std::isfinite(frameRadiusAtFieldPrecision) ||
            frameRadiusAtFieldPrecision != terrainField->radius()) {
        throw std::invalid_argument{
            "local clipmap frame and terrain field radii disagree"};
    }
}

} // namespace

void validateLocalClipmapSamplingConfig(
        const LocalClipmapSamplingConfig& config) {
    if (!std::isfinite(config.curvatureWeight) ||
            config.curvatureWeight < 0.0 || config.curvatureWeight > 1.0) {
        throw std::invalid_argument{
            "local clipmap curvature weight must be between zero and one"};
    }
}

double localClipmapCurvatureWeight(
        const LocalClipmapSamplingConfig& config,
        std::uint32_t level,
        std::uint32_t levelCount) {
    validateLocalClipmapSamplingConfig(config);
    if (levelCount == 0 || level >= levelCount) {
        throw std::out_of_range{
            "local clipmap curvature level is outside the active range"};
    }
    if (config.forceOutermostRingCurved && level > 0 &&
            level + 1 == levelCount) {
        return 1.0;
    }
    return config.curvatureWeight;
}

LocalClipmapSurfaceSample sampleLocalClipmapPoint(
        const LocalPlanetFrame& frame,
        const TerrainFieldSnapshot& terrainField,
        const LocalClipmapConfig& clipmapConfig,
        const LocalClipmapCacheOrigin& cacheOrigin,
        const LocalClipmapGridCoordinate& sampleCoordinate,
        const glm::dvec3& cameraGlobalPositionMeters,
        const LocalClipmapSamplingConfig& samplingConfig) {
    const std::array coordinates{sampleCoordinate};
    std::vector<LocalClipmapSurfaceSample> samples = sampleLocalClipmapPoints(
        frame,
        terrainField,
        clipmapConfig,
        cacheOrigin,
        coordinates,
        cameraGlobalPositionMeters,
        samplingConfig);
    return samples.front();
}

std::vector<LocalClipmapSurfaceSample> sampleLocalClipmapPoints(
        const LocalPlanetFrame& frame,
        const TerrainFieldSnapshot& terrainField,
        const LocalClipmapConfig& clipmapConfig,
        const LocalClipmapCacheOrigin& cacheOrigin,
        std::span<const LocalClipmapGridCoordinate> sampleCoordinates,
        const glm::dvec3& cameraGlobalPositionMeters,
        const LocalClipmapSamplingConfig& samplingConfig) {
    validateSamplingContext(
        frame,
        terrainField,
        clipmapConfig,
        cacheOrigin,
        cameraGlobalPositionMeters,
        samplingConfig);

    const double cellSpacingMeters = localClipmapCellSpacing(
        clipmapConfig,
        cacheOrigin.level);
    const TerrainDetailLevel detail =
        terrainField->detailPolicy().equivalentDetailForSpacing(
            cellSpacingMeters);
    const double curvatureWeight = localClipmapCurvatureWeight(
        samplingConfig,
        cacheOrigin.level,
        clipmapConfig.levelCount);

    std::vector<LocalClipmapSurfaceSample> result;
    result.reserve(sampleCoordinates.size());
    for (const LocalClipmapGridCoordinate& coordinate : sampleCoordinates) {
        const glm::dvec2 localPositionMeters = localClipmapSamplePosition(
            cacheOrigin,
            coordinate,
            cellSpacingMeters);
        const glm::dvec3 globalDirection = localPlanetSurfaceDirection(
            frame,
            localPositionMeters);
        const float heightMeters = terrainField->sample(
            globalDirection,
            detail);
        const glm::dvec3 curvedCameraRelative = localPlanetCurvedPosition(
            frame,
            globalDirection,
            heightMeters) - cameraGlobalPositionMeters;
        const glm::dvec3 tangentFlatCameraRelative =
            localPlanetTangentFlatPosition(
                frame,
                localPositionMeters,
                heightMeters) - cameraGlobalPositionMeters;
        const glm::dvec3 blendedCameraRelative =
            tangentFlatCameraRelative * (1.0 - curvatureWeight) +
            curvedCameraRelative * curvatureWeight;
        if (!isFinite(curvedCameraRelative) ||
                !isFinite(tangentFlatCameraRelative) ||
                !isFinite(blendedCameraRelative)) {
            throw std::overflow_error{
                "local clipmap camera-relative sample position overflow"};
        }

        result.push_back({
            .gridCoordinate = coordinate,
            .localPositionMeters = localPositionMeters,
            .globalDirection = globalDirection,
            .terrainDetail = detail,
            .heightMeters = heightMeters,
            .curvedCameraRelativePositionMeters = curvedCameraRelative,
            .tangentFlatCameraRelativePositionMeters =
                tangentFlatCameraRelative,
            .blendedCameraRelativePositionMeters = blendedCameraRelative,
            .curvatureWeight = curvatureWeight,
        });
    }
    return result;
}

} // namespace wgen
