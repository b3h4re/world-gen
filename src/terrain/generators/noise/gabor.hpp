#pragma once

#include "terrain/generators/generator.hpp"

#include <cstddef>

namespace wgen {


    class GaborNoise : public Generator {
    public:

        GaborNoise(std::size_t dotsPerCell, SeedType seed, float impulseDensity,
            float kernelSpatialExtent, float kernelOscillationFrequency, float oscillationOrientation);

        float noise(std::size_t x, std::size_t y) const override;
        GeneratorCapabilities capabilities() const override;
        // virtual std::string compShader() const override;
        // virtual std::size_t specSize() const override;

    private:
        std::size_t dotsPerCell_;
        float impulseDensity_;
        float kernelSpatialExtent_;
        float kernelOscillationFrequency_;
        float oscillationOrientation_;
    };

}
