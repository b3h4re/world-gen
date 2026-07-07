#include "perlin.hpp"

#include "terrain/utils/hash_random.hpp"

#include <cmath>
#include <stdexcept>

namespace wgen {

PerlinNoise3d::PerlinNoise3d(SeedType seed, float cellSize, FloatFunction funcInterpolate)
        : cellSize_{cellSize}, funcInterpolate_{funcInterpolate} {
    if (cellSize_ <= 0.0F) {
        throw std::invalid_argument("3D Perlin cell size must be positive");
    }
    if (funcInterpolate_ == nullptr) {
        throw std::invalid_argument("Perlin interpolation function must not be null");
    }

    setSeed(seed);
}

float PerlinNoise3d::noise(glm::vec3 point) const {
    const glm::vec3 scaledPoint = point / cellSize_;
    const int gridX = static_cast<int>(std::floor(scaledPoint.x));
    const int gridY = static_cast<int>(std::floor(scaledPoint.y));
    const int gridZ = static_cast<int>(std::floor(scaledPoint.z));

    const glm::vec3 localPoint{
        scaledPoint.x - static_cast<float>(gridX),
        scaledPoint.y - static_cast<float>(gridY),
        scaledPoint.z - static_cast<float>(gridZ)
    };

    const float fadeX = funcInterpolate_(localPoint.x);
    const float fadeY = funcInterpolate_(localPoint.y);
    const float fadeZ = funcInterpolate_(localPoint.z);

    std::vector<float> cornerWeights{};
    cornerWeights.reserve(8);

    // {0, 0, 0} -> 0
    // {0, 0, 1} -> 1
    // {0, 1, 0} -> 2
    // {0, 1, 1} -> 3
    // {1, 0, 0} -> 4
    // {1, 0, 1} -> 5
    // {1, 1, 0} -> 6
    // {1, 1, 1} -> 7
    for (int i = 0; i <= 1; ++i) {
        for (int j = 0; j <= 1; ++j) {
            for (int u = 0; u <= 1; ++u) {
                glm::vec3 randomDir = randomHashDir3D(gridX + i, gridY + j, gridZ + u, getSeed());
                glm::vec3 cornerOffset{
                    static_cast<float>(i),
                    static_cast<float>(j),
                    static_cast<float>(u)
                };
                cornerWeights.push_back(
                    glm::dot(localPoint - cornerOffset, randomDir)
                );
            }
        }
    }

    // (0, 0, 0) - (1, 0, 0)
    const float x00 = lerp(cornerWeights[0], cornerWeights[4], fadeX);
    // (0, 1, 0) - (1, 1, 0)
    const float x10 = lerp(cornerWeights[2], cornerWeights[6], fadeX);
    // (0, 0, 1) - (1, 0, 1)
    const float x01 = lerp(cornerWeights[1], cornerWeights[5], fadeX);
    // (0, 1, 1) - (1, 1, 1)
    const float x11 = lerp(cornerWeights[3], cornerWeights[7], fadeX);

    const float y0 = lerp(x00, x10, fadeY);
    const float y1 = lerp(x01, x11, fadeY);

    return lerp(y0, y1, fadeZ);
}

GeneratorCapabilities PerlinNoise3d::capabilities() const {
    return {
        .cpu = true,
        .vulkanCompute = true,
        .octaves = true,
    };
}

std::string PerlinNoise3d::compShader() const {
    return "perlin_3d";
}

std::size_t PerlinNoise3d::specSize() const {
    return sizeof(PerlinNoiseComputeSpec3D);
}

} // namespace wgen
