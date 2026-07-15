#pragma once

#include "terrain/generators/3d/generator.hpp"
#include "terrain/generators/3d/generator_spec.hpp"
#include "terrain/planet/planet_patch.hpp"
#include "terrain/terrain_detail.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace wgen {

inline constexpr std::size_t TERRAIN_CALIBRATION_RESOLUTION = 65;

struct TerrainHeightCalibration {
    float rawMinimum{};
    float rawMaximum{};
    float scale{};
    float bias{};

    float apply(float rawHeight) const;
};

class TerrainField {
public:
    TerrainField(Generator3dPipelineSpec pipeline, SeedType seed, float radius);

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

    float radius() const { return radius_; }
    float maximumAbsoluteHeight() const { return maximumAbsoluteHeight_; }
    const TerrainHeightCalibration& calibration() const { return calibration_; }
    const TerrainDetailPolicy& detailPolicy() const { return detailPolicy_; }

private:
    struct Contributor {
        std::unique_ptr<Generator3d> generator{};
        float amplitude{};
        TerrainDetailBand detailBand{};
    };

    float sampleRaw(glm::vec3 direction, TerrainDetailLevel detail) const;
    void calibrate();

    std::vector<Contributor> contributors_{};
    float radius_{};
    TerrainDetailPolicy detailPolicy_;
    float maximumAbsoluteRawHeight_{};
    float maximumAbsoluteHeight_{};
    TerrainHeightCalibration calibration_{};
};

using TerrainFieldSnapshot = std::shared_ptr<const TerrainField>;

TerrainFieldSnapshot buildTerrainFieldSnapshot(
    const Generator3dPipelineSpec& pipeline,
    SeedType seed,
    float radius);

} // namespace wgen
