#include "terrain_pipeline.hpp"

#include "terrain/utils/hash_random.hpp"

#include <stdexcept>
#include <utility>

namespace wgen {

void TerrainPipeline3d::push_back(std::unique_ptr<Generator3d> generator) {
    push_back(std::move(generator), multiplyFunction(1.0F));
}

void TerrainPipeline3d::push_back(std::unique_ptr<Generator3d> generator, HeightFunc impact) {
    if (generator == nullptr) {
        throw std::invalid_argument("3D terrain pipeline generator cannot be null");
    }

    generators_.push_back(std::move(generator));
    generatorsImpact_.push_back(std::move(impact));
}

void TerrainPipeline3d::setSeed(SeedType newSeed) {
    Generator3d::setSeed(newSeed);
    for (std::size_t i = 0; i < generators_.size(); ++i) {
        if (i == 0) {
            generators_[i]->setSeed(newSeed);
        } else {
            generators_[i]->setSeed(hashSeed(generators_[i - 1]->getSeed()));
        }
    }
}

float TerrainPipeline3d::noise(glm::vec3 point) const {
    float noiseVal = 0.0F;
    for (std::size_t i = 0; i < generators_.size(); ++i) {
        noiseVal += generatorsImpact_[i](generators_[i]->noise(point));
    }
    return noiseVal;
}

CubeSphere<float> TerrainPipeline3d::generateCubeSphere(std::size_t resolution) const {
    CubeSphere<float> cubeSphere{resolution, 0.0F};
    for (std::size_t i = 0; i < generators_.size(); ++i) {
        cubeSphere += map(generators_[i]->generateCubeSphere(resolution), generatorsImpact_[i]);
    }
    return cubeSphere;
}

} // namespace wgen
