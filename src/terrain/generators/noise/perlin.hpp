#pragma once

#include "terrain/generators/noise/gradient_noise_interface.hpp"

#include <cstddef>

namespace wgen {


    class PerlinNoise2d : public GradientNoise {
    public:
        using FloatFunction = float (*)(float);

        PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, SeedType seed,
                      FloatFunction funcInterpolate = defaultPerlinInterp);
        PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                      FloatFunction funcInterpolate = defaultPerlinInterp);

        float noise(std::size_t x, std::size_t y) const override;
        GeneratorCapabilities capabilities() const override;
        virtual std::string compShader() const override;
        virtual std::size_t specSize() const override;

    private:

        FloatFunction funcInterpolate_;
    };

}
