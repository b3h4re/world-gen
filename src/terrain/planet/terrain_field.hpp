#pragma once

#include "terrain/generators/3d/generator.hpp"
#include "terrain/generators/3d/generator_spec.hpp"
#include "terrain/planet/planet_patch.hpp"

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

    float sample(const PlanetSurfaceSample& surface, std::uint8_t maxDetailLevel) const;
    CubeSphere<float> generateCubeSphere(
        std::size_t resolution,
        std::uint8_t maxDetailLevel) const;

    float radius() const { return radius_; }
    const TerrainHeightCalibration& calibration() const { return calibration_; }

private:
    struct Contributor {
        std::unique_ptr<Generator3d> generator{};
        float amplitude{};
        std::uint8_t firstVisibleLod{};
    };

    float sampleRaw(glm::vec3 direction, std::uint8_t maxDetailLevel) const;
    void calibrate();

    std::vector<Contributor> contributors_{};
    float radius_{};
    TerrainHeightCalibration calibration_{};
};

using TerrainFieldSnapshot = std::shared_ptr<const TerrainField>;

TerrainFieldSnapshot buildTerrainFieldSnapshot(
    const Generator3dPipelineSpec& pipeline,
    SeedType seed,
    float radius);

} // namespace wgen
