#pragma once

#include "terrain/generators/3d/generator.hpp"
#include "terrain/generators/3d/generator_spec.hpp"
#include "terrain/planet/planet_patch.hpp"
#include "terrain/terrain_detail.hpp"
#include "terrain/terrain_height.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace wgen {

inline constexpr std::size_t LEGACY_TERRAIN_CALIBRATION_RESOLUTION = 65;

class TerrainField {
public:
    TerrainField(
        Generator3dPipelineSpec pipeline,
        SeedType seed,
        float radiusMeters,
        TerrainHeightSemantics heightSemantics = TerrainHeightSemantics::PhysicalMeters);

    TerrainField(const TerrainField&) = delete;
    TerrainField& operator=(const TerrainField&) = delete;
    TerrainField(TerrainField&&) noexcept = default;
    TerrainField& operator=(TerrainField&&) noexcept = default;

    float sample(const PlanetSurfaceSample& surface, TerrainDetailLevel detail) const;
    float sample(const PlanetSurfaceSample& surface, std::uint8_t maxDetailLevel) const;
    CubeSphere<float> generateCubeSphere(
        std::size_t resolution,
        TerrainDetailLevel detail) const;
    CubeSphere<float> generateCubeSphere(
        std::size_t resolution,
        std::uint8_t maxDetailLevel) const;

    float radius() const { return radiusMeters_; }
    TerrainHeightSemantics heightSemantics() const { return heightSemantics_; }
    const TerrainHeightBounds& heightBounds() const { return heightBounds_; }
    const TerrainDisplayHeightRange& displayHeightRange() const { return displayHeightRange_; }
    const TerrainDetailPolicy& detailPolicy() const { return detailPolicy_; }

private:
    struct HeightTransform {
        float scale{1.0F};
        float bias{};

        float apply(float authoredHeight) const;
    };

    struct Contributor {
        std::unique_ptr<Generator3d> generator{};
        float amplitude{};
        float bias{};
        float maximumAbsoluteNoiseHeight{};
        float maximumAbsoluteAuthoredHeight{};
        TerrainDetailBand detailBand{};
    };

    float sampleAuthored(glm::dvec3 direction, TerrainDetailLevel detail) const;
    void calibrateLegacyHeightTransform();
    void buildHeightMetadata();

    std::vector<Contributor> contributors_{};
    float radiusMeters_{};
    TerrainHeightSemantics heightSemantics_{TerrainHeightSemantics::PhysicalMeters};
    TerrainDetailPolicy detailPolicy_;
    HeightTransform geometryHeightTransform_{};
    TerrainHeightBounds heightBounds_{};
    TerrainDisplayHeightRange displayHeightRange_{};
};

using TerrainFieldSnapshot = std::shared_ptr<const TerrainField>;

TerrainFieldSnapshot buildTerrainFieldSnapshot(
    const Generator3dPipelineSpec& pipeline,
    SeedType seed,
    float radiusMeters,
    TerrainHeightSemantics heightSemantics = TerrainHeightSemantics::PhysicalMeters);

} // namespace wgen
