#pragma once

#include "terrain/generators/noise/gradient_noise_interface.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wgen {

    class WorleyNoise2d : public GradientNoise {
    public:

        WorleyNoise2d(
            std::size_t gridWidth,
            std::size_t gridHeight,
            std::size_t dotsPerCell,
            std::uint32_t seed,
            float p = 2,
            std::size_t numPoints = 1
        );
        WorleyNoise2d(
            std::size_t gridWidth,
            std::size_t gridHeight,
            std::size_t dotsPerCell,
            float p = 2,
            std::size_t numPoints = 1
        );
        float noise(std::size_t x, std::size_t y) const override;

    private:
        float p_;
        std::size_t numPoints_;
        HeightMap<std::vector<glm::vec2>> featurePoints_;

        // here gradients are feature points
        // void generateGradients() override;
        std::vector<glm::vec2> featurePointsAt(std::size_t i, std::size_t j) const;

        static std::size_t wrapSignedIndex(std::ptrdiff_t index, std::size_t size);
        std::vector<glm::vec2> deltaToAdjacentFeaturePoints(std::size_t x, std::size_t y, std::ptrdiff_t i, std::ptrdiff_t j) const;

    };

}
