#pragma once


#include "terrain/generators/generator.hpp"


namespace wgen {

struct DLAGeneratorConfig {
    std::size_t numSteps{5};
    std::uint32_t seed{0};
    HeightFunc heightFunc = defaultDLAHeightFunction(1.0F);
    float fill = 0.25;
    float jiggle = 0.021;
    std::size_t width{100};
    std::size_t height{100};
};

class DLACombiner : public Generator {
public:
    DLACombiner(std::size_t numDlas, std::vector<DLAGeneratorConfig> configs = {});
    DLACombiner(std::size_t numDlas, std::uint32_t seed, std::vector<DLAGeneratorConfig> configs = {});

    HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;

    void setSeed(std::uint32_t newSeed) override;

private:
    std::size_t numDlas_;
    std::vector<DLAGeneratorConfig> configs_;
    std::vector<bool> presetSeeds;
    std::vector<HeightMap<float>> dlaResults;


    void normalizeConfigs();
    void generateDlas();

    float noise(std::size_t x, std::size_t y) const override;

};


}
