#include "dla_combiner.hpp"
#include "dla.hpp"


namespace wgen {
    DLACombiner::DLACombiner(std::size_t numDlas, std::vector<DLAGeneratorConfig> configs)
        : DLACombiner{numDlas, std::random_device{}(), configs} {}


    DLACombiner::DLACombiner(std::size_t numDlas, std::uint32_t seed, std::vector<DLAGeneratorConfig> configs)
        : numDlas_{numDlas}, configs_{configs} {
        Generator::setSeed(seed);
        presetSeeds.reserve(numDlas_);
        presetSeeds.assign(numDlas, false);
        normalizeConfigs();
        setSeed(seed);
    }

    void DLACombiner::setSeed(std::uint32_t newSeed) {
        Generator::setSeed(newSeed);
        for (std::size_t i = 0; i < numDlas_; ++i) {
            if (presetSeeds[0]) { continue; }
            configs_[i].seed = i == 0 ? hashSeed(getSeed()) : hashSeed(configs_[i - 1].seed);
        }
        generateDlas();
    }

    void DLACombiner::normalizeConfigs() {
        while (configs_.size() > numDlas_) {
            configs_.pop_back();
        }
        for (std::size_t c = 0; c < configs_.size(); ++c) {
            if (configs_[c].seed == 0) {
                configs_[c].seed = c == 0 ? hashSeed(getSeed()) : hashSeed(configs_[c - 1].seed);
            }
            presetSeeds[c] = configs_[c].seed == 0;
        }
        while (configs_.size() < numDlas_) {
            DLAGeneratorConfig cfg = configs_.size() == 0 ?
                DLAGeneratorConfig{} : configs_[configs_.size() - 1];

            cfg.seed = configs_.size() == 0 ? hashSeed(getSeed()) : hashSeed(configs_[configs_.size() - 1].seed);
            presetSeeds[configs_.size()] = configs_.size() == 0 ?
                false : presetSeeds[configs_.size() - 1];
            configs_.push_back(cfg);
        }
    }


    void DLACombiner::generateDlas() {
        dlaResults.clear();

        for (std::size_t i = 0; i < numDlas_; ++i) {
            DLADualFilterBlur dla{
                configs_[i].numSteps,
                configs_[i].seed,
                configs_[i].heightFunc,
                configs_[i].fill,
                configs_[i].jiggle
            };

            dlaResults.push_back(
                dla.generateHeightMap(configs_[i].width, configs_[i].height)
            );
        }
    }

    HeightMap<float> DLACombiner::generateHeightMap(std::size_t width, std::size_t height) const {
        HeightMap<float> res{width, height};

        std::mt19937 rd{getSeed()};

        for (std::size_t i = 0; i < numDlas_; ++i) {
            std::size_t posX = rd() % width;
            std::size_t posY = rd() % width;
            res.add_at(dlaResults[i], posX, posY);
        }

        return res;
    }

    float DLACombiner::noise(std::size_t x, std::size_t y) const {
        return 0.0F;
    }
}
