#include "perlin.hpp"

#include "terrain/utils/hash_random.hpp"

#include <cmath>
#include <array>
#include <limits>
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

float PerlinNoise3d::noise(glm::dvec3 point) const {
    const glm::dvec3 scaledPoint = point / cellSize_;
    if (!std::isfinite(scaledPoint.x) || !std::isfinite(scaledPoint.y) ||
            !std::isfinite(scaledPoint.z) ||
            scaledPoint.x < static_cast<double>(std::numeric_limits<int>::min()) ||
            scaledPoint.x > static_cast<double>(std::numeric_limits<int>::max() - 1) ||
            scaledPoint.y < static_cast<double>(std::numeric_limits<int>::min()) ||
            scaledPoint.y > static_cast<double>(std::numeric_limits<int>::max() - 1) ||
            scaledPoint.z < static_cast<double>(std::numeric_limits<int>::min()) ||
            scaledPoint.z > static_cast<double>(std::numeric_limits<int>::max() - 1)) {
        throw std::invalid_argument{"3D Perlin sample coordinate is invalid"};
    }
    const int gridX = static_cast<int>(std::floor(scaledPoint.x));
    const int gridY = static_cast<int>(std::floor(scaledPoint.y));
    const int gridZ = static_cast<int>(std::floor(scaledPoint.z));

    const glm::dvec3 localPoint{
        scaledPoint.x - static_cast<double>(gridX),
        scaledPoint.y - static_cast<double>(gridY),
        scaledPoint.z - static_cast<double>(gridZ)
    };

    const auto interpolate = [this](double value) {
        if (funcInterpolate_ == defaultPerlinInterp) {
            return value * value * value *
                (value * (value * 6.0 - 15.0) + 10.0);
        }
        return static_cast<double>(funcInterpolate_(static_cast<float>(value)));
    };
    const double fadeX = interpolate(localPoint.x);
    const double fadeY = interpolate(localPoint.y);
    const double fadeZ = interpolate(localPoint.z);

    std::array<double, 8> cornerWeights{};
    std::size_t cornerIndex = 0;

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
                const glm::dvec3 randomDirection{
                    randomHashDir3D(gridX + i, gridY + j, gridZ + u, getSeed())};
                const glm::dvec3 cornerOffset{
                    static_cast<double>(i),
                    static_cast<double>(j),
                    static_cast<double>(u)
                };
                cornerWeights[cornerIndex++] = glm::dot(
                    localPoint - cornerOffset,
                    randomDirection);
            }
        }
    }

    // (0, 0, 0) - (1, 0, 0)
    const double x00 = std::lerp(cornerWeights[0], cornerWeights[4], fadeX);
    // (0, 1, 0) - (1, 1, 0)
    const double x10 = std::lerp(cornerWeights[2], cornerWeights[6], fadeX);
    // (0, 0, 1) - (1, 0, 1)
    const double x01 = std::lerp(cornerWeights[1], cornerWeights[5], fadeX);
    // (0, 1, 1) - (1, 1, 1)
    const double x11 = std::lerp(cornerWeights[3], cornerWeights[7], fadeX);

    const double y0 = std::lerp(x00, x10, fadeY);
    const double y1 = std::lerp(x01, x11, fadeY);

    return static_cast<float>(std::lerp(y0, y1, fadeZ));
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
