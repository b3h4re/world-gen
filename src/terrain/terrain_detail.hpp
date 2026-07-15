#pragma once

#include <cstdint>

namespace wgen {

inline constexpr std::uint8_t MAX_TERRAIN_DETAIL_LEVEL = 24;
inline constexpr std::uint32_t DEFAULT_TERRAIN_DETAIL_REFERENCE_QUADS = 32;

// Renderer-independent continuous terrain detail coordinate. Integer values
// preserve the existing detail-band behavior; fractional values blend the next
// band without changing render-patch identity.
class TerrainDetailLevel {
public:
    explicit TerrainDetailLevel(double value);

    static TerrainDetailLevel fromDiscrete(std::uint8_t level);

    double value() const { return value_; }
    bool operator==(const TerrainDetailLevel&) const = default;

private:
    double value_{};
};

struct TerrainDetailBand {
    TerrainDetailLevel fadeStart{0.0};
    TerrainDetailLevel fullyVisible{0.0};
};

class TerrainDetailPolicy {
public:
    TerrainDetailPolicy(
        double planetRadius,
        std::uint32_t referenceQuadsPerFace = DEFAULT_TERRAIN_DETAIL_REFERENCE_QUADS);

    double referenceSampleSpacing() const { return referenceSampleSpacing_; }
    double sampleSpacingForDetail(TerrainDetailLevel detail) const;
    double cubeFacePatchSampleSpacing(
        std::uint8_t patchLevel,
        std::uint32_t patchQuadCount) const;
    TerrainDetailLevel equivalentDetailForSpacing(double metersPerSample) const;
    TerrainDetailLevel detailForCubeFacePatch(
        std::uint8_t patchLevel,
        std::uint32_t patchQuadCount) const;

private:
    double planetRadius_{};
    double referenceSampleSpacing_{};
};

TerrainDetailBand terrainDetailBandForFirstFullyVisibleLevel(std::uint8_t level);
float terrainDetailBandWeight(
    TerrainDetailLevel requestedDetail,
    const TerrainDetailBand& band);

} // namespace wgen
