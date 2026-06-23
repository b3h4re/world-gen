#pragma once

#include "terrain/generators/noise/gradient_noise_interface.hpp"

namespace wgen {


    class PerlinNoise2d : public GradientNoise {
    public:
        using FloatFunction = float (*)(float);

        PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, std::uint32_t seed,
                      FloatFunction funcInterpolate = defaultPerlinInterp);
        PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                      FloatFunction funcInterpolate = defaultPerlinInterp);

    private:
        float noise(std::size_t x, std::size_t y) const override;

        FloatFunction funcInterpolate_;
    };

}
