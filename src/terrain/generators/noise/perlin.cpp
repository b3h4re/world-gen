#include "perlin.hpp"
#include "terrain/utils/hash_random.hpp"

#include <random>
#include <stdexcept>

namespace wgen {

    PerlinNoise2d::PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                                 FloatFunction funcInterpolate)
    : PerlinNoise2d{gridWidth, gridHeight, dotsPerCell, std::random_device{}(), funcInterpolate} {}

    PerlinNoise2d::PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                                 SeedType seed, FloatFunction funcInterpolate)
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

        glm::vec2 randomDirXY = randomHashDir(gridX, gridY, getSeed());
        glm::vec2 randomDirX1Y = randomHashDir(gridX + 1, gridY, getSeed());
        glm::vec2 randomDirXY1 = randomHashDir(gridX, gridY + 1, getSeed());
        glm::vec2 randomDirX1Y1 = randomHashDir(gridX+1, gridY + 1, getSeed());

        const float topLeft = glm::dot(point, randomDirXY);
        const float topRight = glm::dot(point - glm::vec2{1.0F, 0.0F}, randomDirX1Y);
        const float bottomLeft = glm::dot(point - glm::vec2{0.0F, 1.0F}, randomDirXY1);
        const float bottomRight = glm::dot(
            point - glm::vec2{1.0F, 1.0F},
            randomDirX1Y1
        );

        const float fadeX = funcInterpolate_(localX);
        const float fadeY = funcInterpolate_(localY);
        const float top = lerp(topLeft, topRight, fadeX);
        const float bottom = lerp(bottomLeft, bottomRight, fadeX);
        return lerp(top, bottom, fadeY);
    }

    GeneratorCapabilities PerlinNoise2d::capabilities() const {
        return {
            .cpu = true,
            .vulkanCompute = true,
        };
    }

    std::string PerlinNoise2d::compShader() const {
        return "perlin_noise";
    }

    std::size_t PerlinNoise2d::specSize() const {
        return sizeof(PerlinNoiseComputeSpec);
    }
}
