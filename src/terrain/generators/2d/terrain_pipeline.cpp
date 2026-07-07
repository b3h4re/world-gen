#include "terrain_pipeline.hpp"

#include "terrain/utils/hash_random.hpp"

#include <stdexcept>
#include <utility>



namespace wgen {

void TerrainPipeline::push_back(std::unique_ptr<Generator> generator) {
    push_back(std::move(generator), multiplyFunction(1.0F));
}

void TerrainPipeline::push_back(std::unique_ptr<Generator> generator, HeightFunc impact) {
    if (generator == nullptr) {
        throw std::invalid_argument("terrain pipeline generator cannot be null");
    }

    generators_.push_back(std::move(generator));
    generatorsImpact_.push_back(std::move(impact));
}

void TerrainPipeline::setSeed(SeedType newSeed)  {
    Generator::setSeed(newSeed);
    for (std::size_t i = 0; i < generators_.size(); ++i) {
        if (i == 0) {
            generators_[i]->setSeed(newSeed);
        } else {
            generators_[i]->setSeed(
                hashSeed(generators_[i - 1]->getSeed())
            );
        }
    }
}

float TerrainPipeline::noise(std::size_t x, std::size_t y) const {
    float noiseVal = 0.0F;
    for (std::size_t i = 0; i < generators_.size(); ++i) {
        noiseVal += generatorsImpact_[i](
            generators_[i]->noise(x, y)
        );
    }
    return noiseVal;
}

HeightMap<float> TerrainPipeline::generateHeightMap(std::size_t width, std::size_t height) const {
    HeightMap<float> res{width, height};
    for (std::size_t i = 0; i < generators_.size(); ++i) {
        res += map(generators_[i]->generateHeightMap(width, height), generatorsImpact_[i]);
    }
    return res;
}

}
