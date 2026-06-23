#pragma once

#include "terrain/generators/noise/gradient_noise_interface.hpp"

namespace wgen {

    class WorleyNoise2d : public GradientNoise {
    public:

        WorleyNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, std::uint32_t seed);
        WorleyNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell);

    private:
        float noise(std::size_t x, std::size_t y) const override;
        // here gradients are feature points
        void generateGradients() override;
        glm::vec2 featurePointAt(std::size_t i, std::size_t j) const;

    };

}
