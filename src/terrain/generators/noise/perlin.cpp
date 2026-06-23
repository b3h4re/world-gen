#include "perlin.hpp"

#include <array>
#include <limits>
#include <random>
#include <stdexcept>

namespace wgen {
    namespace {

        // So basically a bunch of unit vectors
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

    }

    float PerlinNoise2d::lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    float defaultPerlinInterp(float t) {
        return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
    }

    PerlinNoise2d::PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                                 FloatFunction funcInterpolate)
    : PerlinNoise2d{gridWidth, gridHeight, dotsPerCell, std::random_device{}(), funcInterpolate} {}

    PerlinNoise2d::PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                                 std::uint32_t seed, FloatFunction funcInterpolate)
    : funcInterpolate_{funcInterpolate}, gridWidth_{gridWidth}, gridHeight_{gridHeight},
      dotsPerCell_{dotsPerCell}, gradients_{gridWidth, gridHeight} {
        if (dotsPerCell_ < 2) {
            throw std::invalid_argument("dots per cell must be at least two");
        }
        if (funcInterpolate_ == nullptr) {
            throw std::invalid_argument("Perlin interpolation function must not be null");
        }
        if (dotsPerCell_ > std::numeric_limits<std::size_t>::max() / (gridWidth_ - 1) ||
            dotsPerCell_ > std::numeric_limits<std::size_t>::max() / (gridHeight_ - 1)) {
            throw std::overflow_error("Perlin sample dimensions overflow size_t");
        }

        setSeed(seed);
    }

    void PerlinNoise2d::setSeed(std::uint32_t newSeed) {
        Generator::setSeed(newSeed);
        generateGradients();
    }

    void PerlinNoise2d::generateGradients() {
        std::mt19937 random{getSeed()};

        for (std::size_t y = 0; y < gridHeight_; ++y) {
            for (std::size_t x = 0; x < gridWidth_; ++x) {
                const std::size_t directionIndex = random() % GRADIENT_DIRECTIONS.size();
                gradients_.at(x, y) = GRADIENT_DIRECTIONS[directionIndex];
            }
        }
    }

    float PerlinNoise2d::noise(std::size_t x, std::size_t y) const {
        if (x >= sampleWidth() || y >= sampleHeight()) {
            throw std::out_of_range("Perlin sample coordinate is outside the gradient grid");
        }

        const std::size_t gridX = x / dotsPerCell_;
        const std::size_t gridY = y / dotsPerCell_;
        const float localX = static_cast<float>(x % dotsPerCell_) / static_cast<float>(dotsPerCell_);
        const float localY = static_cast<float>(y % dotsPerCell_) / static_cast<float>(dotsPerCell_);
        const glm::vec2 point{localX, localY};

        const float topLeft = glm::dot(point, gradients_.at(gridX, gridY));
        const float topRight = glm::dot(point - glm::vec2{1.0F, 0.0F}, gradients_.at(gridX + 1, gridY));
        const float bottomLeft = glm::dot(point - glm::vec2{0.0F, 1.0F}, gradients_.at(gridX, gridY + 1));
        const float bottomRight = glm::dot(
            point - glm::vec2{1.0F, 1.0F},
            gradients_.at(gridX + 1, gridY + 1)
        );

        const float fadeX = funcInterpolate_(localX);
        const float fadeY = funcInterpolate_(localY);
        const float top = lerp(topLeft, topRight, fadeX);
        const float bottom = lerp(bottomLeft, bottomRight, fadeX);
        return lerp(top, bottom, fadeY);
    }

    std::size_t PerlinNoise2d::sampleWidth() const {
        return (gridWidth_ - 1) * dotsPerCell_;
    }

    std::size_t PerlinNoise2d::sampleHeight() const {
        return (gridHeight_ - 1) * dotsPerCell_;
    }

    HeightMap<float> PerlinNoise2d::generateHeightMap(std::size_t width, std::size_t height) {
        if (width > sampleWidth() || height > sampleHeight()) {
            throw std::invalid_argument("requested height map exceeds the Perlin gradient grid");
        }

        HeightMap<float> map{width, height};
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                map.at(x, y) = noise(x, y);
            }
        }

        return map;
    }

}
