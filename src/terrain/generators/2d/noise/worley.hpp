#pragma once

#include "terrain/generators/2d/noise/gradient_noise_interface.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wgen {

    class WorleyNoise2d : public Generator {
    public:
        static constexpr std::size_t MIN_GPU_FEATURE_POINT_COUNT{1};

        WorleyNoise2d(
            std::size_t dotsPerCell,
            SeedType seed,
            float p = 2,
            std::size_t numPoints = 1
        );
        WorleyNoise2d(
            std::size_t dotsPerCell,
            float p = 2,
            std::size_t numPoints = 1
        );
        float noise(std::size_t x, std::size_t y) const override;
        GeneratorCapabilities capabilities() const override;
        virtual std::string compShader() const override;
        virtual std::size_t specSize() const override;

    private:
        float p_;
        std::size_t dotsPerCell_;
        std::size_t numPoints_;

        // here gradients are feature points
        // void generateGradients() override;
        std::vector<glm::vec2> featurePointsAt(std::size_t i, std::size_t j) const;

        static std::size_t wrapSignedIndex(std::ptrdiff_t index, std::size_t size);
        std::vector<glm::vec2> deltaToAdjacentFeaturePoints(std::size_t x, std::size_t y, std::ptrdiff_t i, std::ptrdiff_t j) const;

    };

}
