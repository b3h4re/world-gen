#include "terrain_detail.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace wgen {

TerrainDetailLevel::TerrainDetailLevel(double value) : value_{value} {
    if (!std::isfinite(value_) || value_ < 0.0 ||
            value_ > static_cast<double>(MAX_TERRAIN_DETAIL_LEVEL)) {
        throw std::invalid_argument{"terrain detail level is outside the supported range"};
    }
}

TerrainDetailLevel TerrainDetailLevel::fromDiscrete(std::uint8_t level) {
    if (level > MAX_TERRAIN_DETAIL_LEVEL) {
        throw std::invalid_argument{"discrete terrain detail level exceeds the supported maximum"};
    }
    return TerrainDetailLevel{static_cast<double>(level)};
}

TerrainDetailPolicy::TerrainDetailPolicy(
        double planetRadius,
        std::uint32_t referenceQuadsPerFace)
    : planetRadius_{planetRadius} {
    if (!std::isfinite(planetRadius_) || planetRadius_ <= 0.0) {
        throw std::invalid_argument{"terrain detail planet radius must be finite and positive"};
    }
    if (referenceQuadsPerFace == 0) {
        throw std::invalid_argument{"terrain detail reference quad count must be positive"};
    }

    constexpr double CUBE_FACE_ANGULAR_SPAN = std::numbers::pi / 2.0;
    referenceSampleSpacing_ = planetRadius_ * CUBE_FACE_ANGULAR_SPAN /
        static_cast<double>(referenceQuadsPerFace);
}

double TerrainDetailPolicy::sampleSpacingForDetail(TerrainDetailLevel detail) const {
    return referenceSampleSpacing_ * std::exp2(-detail.value());
}

double TerrainDetailPolicy::cubeFacePatchSampleSpacing(
        std::uint8_t patchLevel,
        std::uint32_t patchQuadCount) const {
    if (patchQuadCount == 0) {
        throw std::invalid_argument{"cube-face patch quad count must be positive"};
    }

    constexpr double CUBE_FACE_ANGULAR_SPAN = std::numbers::pi / 2.0;
    const double levelScale = std::ldexp(1.0, static_cast<int>(patchLevel));
    return planetRadius_ * CUBE_FACE_ANGULAR_SPAN /
        (static_cast<double>(patchQuadCount) * levelScale);
}

TerrainDetailLevel TerrainDetailPolicy::equivalentDetailForSpacing(double metersPerSample) const {
    if (!std::isfinite(metersPerSample) || metersPerSample <= 0.0) {
        throw std::invalid_argument{"terrain sample spacing must be finite and positive"};
    }

    const double unclamped = std::log2(referenceSampleSpacing_ / metersPerSample);
    return TerrainDetailLevel{std::clamp(
        unclamped,
        0.0,
        static_cast<double>(MAX_TERRAIN_DETAIL_LEVEL))};
}

TerrainDetailLevel TerrainDetailPolicy::detailForCubeFacePatch(
        std::uint8_t patchLevel,
        std::uint32_t patchQuadCount) const {
    return equivalentDetailForSpacing(
        cubeFacePatchSampleSpacing(patchLevel, patchQuadCount));
}

TerrainDetailBand terrainDetailBandForFirstFullyVisibleLevel(std::uint8_t level) {
    const TerrainDetailLevel fullyVisible = TerrainDetailLevel::fromDiscrete(level);
    const double fadeStart = level == 0 ? 0.0 : static_cast<double>(level - 1);
    return {
        .fadeStart = TerrainDetailLevel{fadeStart},
        .fullyVisible = fullyVisible,
    };
}

float terrainDetailBandWeight(
        TerrainDetailLevel requestedDetail,
        const TerrainDetailBand& band) {
    const double start = band.fadeStart.value();
    const double end = band.fullyVisible.value();
    if (end < start) {
        throw std::invalid_argument{"terrain detail band ends before it starts"};
    }
    if (end == start) {
        return requestedDetail.value() >= end ? 1.0F : 0.0F;
    }

    const double linearWeight = std::clamp(
        (requestedDetail.value() - start) / (end - start),
        0.0,
        1.0);
    const double smoothWeight = linearWeight * linearWeight * (3.0 - 2.0 * linearWeight);
    return static_cast<float>(smoothWeight);
}

} // namespace wgen
