#pragma once

#include "terrain/generators/generator.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>


namespace wgen {

    // So basically a bunch of unit vectors
    // For random gradient directions in noises
    constexpr float SQRT_3 = 1.7320508075688772F;
    constexpr float SQRT_2 = 1.4142135623730951F;
    constexpr float SQRT_5 = 2.2360679774997897F;
    constexpr std::array<glm::vec2, 32> GRADIENT_DIRECTIONS{{
        {1.0F, 0.0F},
        {SQRT_3 / 2.0f, 0.5F},
        {SQRT_2 / 2, SQRT_2 / 2},
        {0.5F, SQRT_3 / 2.0f},
        {0.0F, 1.0F},
        {-0.5F, SQRT_3 / 2.0f},
        {-SQRT_2 / 2, SQRT_2 / 2},
        {-SQRT_3 / 2.0f, 0.5F},
        {-1.0F, 0.0F},
        {-SQRT_3 / 2.0f, -0.5F},
        {-SQRT_2 / 2, -SQRT_2 / 2},
        {-0.5F, -SQRT_3 / 2.0f},
        {0.0F, -1.0F},
        {0.5F, -SQRT_3 / 2.0f},
        {SQRT_2 / 2, -SQRT_2 / 2},
        {SQRT_3 / 2.0f, -0.5F},
        // 3/5 and 4/5
        {0.6F, 0.8F},
        {0.6F, -0.8F},
        {-0.6F, 0.8F},
        {-0.6F, -0.8F},
        {0.8F, 0.6F},
        {0.8F, -0.6F},
        {-0.8F, 0.6F},
        {-0.8F, -0.6F},
        // 1/sqrt{5} and 2/sqrt{5}
        {1.0F / SQRT_5, 2.0F / SQRT_5},
        {1.0F / SQRT_5, -2.0F / SQRT_5},
        {-1.0F / SQRT_5, 2.0F / SQRT_5},
        {-1.0F / SQRT_5, -2.0F / SQRT_5},
        {2.0F / SQRT_5, 1.0F / SQRT_5},
        {2.0F / SQRT_5, -1.0F / SQRT_5},
        {-2.0F / SQRT_5, 1.0F / SQRT_5},
        {-2.0F / SQRT_5, -1.0F / SQRT_5},
    }};

    class GradientNoise : public Generator {
    public:
        GradientNoise(
            std::size_t gridWidth,
            std::size_t gridHeight,
            std::size_t dotsPerCell,
            std::uint32_t seed
        ) : gridWidth_{gridWidth}, gridHeight_{gridHeight}, dotsPerCell_{dotsPerCell} {
            if (dotsPerCell_ < 2) {
                throw std::invalid_argument("dots per cell must be at least two");
            }
            if (dotsPerCell_ > std::numeric_limits<std::size_t>::max() / (gridWidth_ - 1) ||
                dotsPerCell_ > std::numeric_limits<std::size_t>::max() / (gridHeight_ - 1)) {
                throw std::overflow_error("gradient noise sample dimensions overflow size_t");
            }

            setSeed(seed);
        }

        void setSeed(std::uint32_t newSeed) override {
            Generator::setSeed(newSeed);
        }

        float noise(std::size_t x, std::size_t y) const override = 0;

    protected:
        std::size_t sampleWidth() const { return (gridWidth_ - 1) * dotsPerCell_; }
        std::size_t sampleHeight() const { return (gridHeight_ - 1) * dotsPerCell_; }

        std::size_t gridWidth_;
        std::size_t gridHeight_;
        std::size_t dotsPerCell_;
        // virtual void generateGradients() {
        //     std::mt19937 random{getSeed()};

        //     for (std::size_t y = 0; y < gridHeight_; ++y) {
        //         for (std::size_t x = 0; x < gridWidth_; ++x) {
        //             const std::size_t directionIndex = random() % GRADIENT_DIRECTIONS.size();
        //             gradients_.at(x, y) = GRADIENT_DIRECTIONS[directionIndex];
        //         }
        //     }
        // }
    };

}
