#include "generator.hpp"

#include <cmath>

namespace wgen {

    float minkowskiDistance(glm::vec2 v1, glm::vec2 v2, float p) {
        return std::pow(
                std::pow(std::abs(v1.x - v2.x), p) + std::pow(std::abs(v1.y - v2.y), p),
                1.0F/p
            );
    }

    std::size_t wrapIndex(std::size_t index, std::size_t size) {
        return index % size;
    }

    float defaultPerlinInterp(float t) {
        return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
    }

    float lerp(float a, float b, float c) { return a + c * (b - a); }

    float defaultReconstructionKernel(float t) {
        if (std::abs(t) < 1) {
            return 1 - std::abs(t);
        }
        return 0;
    }

    bool isInside(const glm::ivec2 pos, const glm::ivec2 dir, const std::size_t width, const std::size_t height) {
        return isInsideRectangle(pos, dir, {0, 0}, {width-1, height-1});
    }


    bool isInsideRectangle(const glm::ivec2 pos, const glm::ivec2 dir, glm::ivec2 cpos1, glm::ivec2 cpos2) {
        glm::ivec2 newPos = pos + dir;
        glm::ivec2 corner1{
            std::min(cpos1.x, cpos2.x),
            std::min(cpos1.y, cpos2.y)
        };
        glm::ivec2 corner2{
            std::max(cpos1.x, cpos2.x),
            std::max(cpos1.y, cpos2.y)
        };

        return corner1.x <= newPos.x && newPos.x <= corner2.x && corner1.y <= newPos.y && newPos.y <= corner2.y;
    }

    std::uint32_t hashSeed(std::uint32_t seed) {
        const std::uint64_t hashed = splitmix64(seed);
        return static_cast<std::uint32_t>(hashed ^ (hashed >> 32));
    }

    glm::vec2 randomHashDir(std::size_t x, std::size_t y, std::uint32_t seed) {
        std::uint64_t h = static_cast<std::uint64_t>(seed);
        h ^= splitmix64(static_cast<std::uint64_t>(x) + 0x9E3779B97F4A7C15ull);
        h ^= splitmix64(static_cast<std::uint64_t>(y) + 0xBF58476D1CE4E5B9ull);

        h = splitmix64(h);

        const double u = static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0);
        const double angle = u * 2.0 * std::numbers::pi_v<double>;
        return {
            static_cast<float>(std::cos(angle)),
            static_cast<float>(std::sin(angle))
        };
    }

}
