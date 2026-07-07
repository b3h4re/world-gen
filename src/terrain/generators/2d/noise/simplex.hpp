#pragma once

#include "terrain/generators/2d/noise/gradient_noise_interface.hpp"

#include <cstddef>
#include <cstdint>

namespace wgen {

    class SimplexNoise2d : public Generator {
    public:
        constexpr static float SKEWING_CONSTANT_F2 = (SQRT_3 - 1.0F) / 2.0F;
        constexpr static float UNSKEWING_CONSTANT_G2 = (3.0F - SQRT_3) / 6.0F;
        constexpr static float NORMALIZATION_CONSTANT = 70.0F;

        SimplexNoise2d(std::size_t dotsPerCell, SeedType seed);
        SimplexNoise2d(std::size_t dotsPerCell);
        float noise(std::size_t x, std::size_t y) const override;
        GeneratorCapabilities capabilities() const override;
        virtual std::string compShader() const override;
        virtual std::size_t specSize() const override;

    private:
        std::size_t dotsPerCell_;
    };

}
