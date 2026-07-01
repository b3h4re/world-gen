#pragma once

#include "terrain/generators/generator.hpp"

#include <cstddef>

namespace wgen {


    class PerlinNoise2d : public Generator {
    public:
        using FloatFunction = float (*)(float);

        PerlinNoise2d(
            std::size_t dotsPerCell,
            SeedType seed,
            FloatFunction funcInterpolate = defaultPerlinInterp);

        float noise(std::size_t x, std::size_t y) const override;
        GeneratorCapabilities capabilities() const override;
        virtual std::string compShader() const override;
        virtual std::size_t specSize() const override;

    private:
        std::size_t dotsPerCell_;
        FloatFunction funcInterpolate_;
    };

}
