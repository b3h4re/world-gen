#pragma once

#include "terrain/terrain_detail.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace wgen {

// Planet radii and physical terrain heights use meters. LegacyNormalized
// preserves the old authoring contract in which generator amplitudes were
// normalized through a frozen global calibration before becoming geometry.
enum class TerrainHeightSemantics : std::uint8_t {
    PhysicalMeters,
    LegacyNormalized,
};

struct TerrainDisplayHeightRange {
    float minimumMeters{-1.0F};
    float maximumMeters{1.0F};

    float normalize(float heightMeters) const {
        if (!std::isfinite(heightMeters)) {
            throw std::invalid_argument{"display height must be finite"};
        }
        const float extent = maximumMeters - minimumMeters;
        if (!std::isfinite(extent) || extent <= 0.0F) {
            throw std::logic_error{"display height range must be finite and increasing"};
        }
        return (heightMeters - minimumMeters) / extent;
    }
};

struct TerrainHeightBounds {
    float minimumDisplacementMeters{};
    float maximumDisplacementMeters{};
    float maximumAbsoluteDisplacementMeters{};
    std::array<float, static_cast<std::size_t>(MAX_TERRAIN_DETAIL_LEVEL) + 1>
        maximumOmittedDetailErrorMeters{};

    float omittedDetailErrorMeters(TerrainDetailLevel detail) const {
        // Integer detail levels are the current renderer contract. For a
        // fractional request, retaining the preceding integer's error is a
        // conservative bound throughout the fade interval.
        const auto level = static_cast<std::size_t>(std::floor(detail.value()));
        return maximumOmittedDetailErrorMeters.at(level);
    }
};

} // namespace wgen
