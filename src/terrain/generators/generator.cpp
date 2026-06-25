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
        glm::ivec2 newPos = pos + dir;
        return !(newPos.x > width || newPos.y > height || newPos.x < 0 || newPos.y < 0);
    }

}
