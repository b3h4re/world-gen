#include "perlin.hpp"

#include <random>
#include <stdexcept>

namespace wgen {

    PerlinNoise2d::PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                                 FloatFunction funcInterpolate)
    : PerlinNoise2d{gridWidth, gridHeight, dotsPerCell, std::random_device{}(), funcInterpolate} {}

    PerlinNoise2d::PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                                 std::uint32_t seed, FloatFunction funcInterpolate)
    : GradientNoise{gridWidth, gridHeight, dotsPerCell, seed}, funcInterpolate_{funcInterpolate} {
        if (funcInterpolate_ == nullptr) {
            throw std::invalid_argument("Perlin interpolation function must not be null");
        }
    }

    float PerlinNoise2d::noise(std::size_t x, std::size_t y) const {
        if (x >= sampleWidth() || y >= sampleHeight()) {
            throw std::invalid_argument("Perlin sample coordinate is outside the gradient grid");
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

}
