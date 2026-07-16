#pragma once

#include "generator.hpp"

#include <memory>
#include <vector>

namespace wgen {

class TerrainPipeline3d : public Generator3d {
public:
    TerrainPipeline3d() = default;

    void setSeed(SeedType newSeed) override;
    void push_back(std::unique_ptr<Generator3d> generator);
    void push_back(std::unique_ptr<Generator3d> generator, HeightFunc impact);

    using Generator3d::noise;
    float noise(glm::dvec3 point) const override;
    CubeSphere<float> generateCubeSphere(std::size_t resolution) const override;

private:
    std::vector<std::unique_ptr<Generator3d>> generators_{};
    std::vector<HeightFunc> generatorsImpact_{};
};

} // namespace wgen
