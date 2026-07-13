#pragma once

#include "terrain/terrain.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace wgen {

enum class CubeSphereFace {
    Top,     // +Z
    Bottom,  // -Z
    Right,   // +X
    Left,    // -X
    Back,    // -Y
    Front    // +Y
};

inline constexpr std::array<CubeSphereFace, 6> FACES = {
    CubeSphereFace::Top,
    CubeSphereFace::Bottom,
    CubeSphereFace::Right,
    CubeSphereFace::Left,
    CubeSphereFace::Back,
    CubeSphereFace::Front,
};

std::size_t faceID(CubeSphereFace face);

glm::vec3 spherifyCube(glm::vec3 p);
glm::vec3 cubeSphereDirection(CubeSphereFace face, float u, float v);


// Six square face maps are stacked vertically in faceID order.
template <class T>
class CubeSphere : public HeightMap<T> {
public:
    CubeSphere() = default;
    explicit CubeSphere(std::size_t resolution)
        : HeightMap<T>(resolution, stackedHeight(resolution)), resolution_{resolution} {}
    CubeSphere(float radius, std::size_t resolution)
        : HeightMap<T>(resolution, stackedHeight(resolution)), resolution_{resolution}, radius_{radius} {
        validateRadius(radius_);
    }
    CubeSphere(std::size_t resolution, const T& defaultValue)
        : HeightMap<T>(resolution, stackedHeight(resolution), defaultValue), resolution_{resolution} {}
    CubeSphere(float radius, std::size_t resolution, const T& defaultValue)
        : HeightMap<T>(resolution, stackedHeight(resolution), defaultValue),
          resolution_{resolution},
          radius_{radius} {
        validateRadius(radius_);
    }

    explicit CubeSphere(const HeightMap<T>& heightMap)
        : HeightMap<T>(heightMap), resolution_{heightMap.width()} {
        validateDimensions();
    }

    explicit CubeSphere(HeightMap<T>&& heightMap)
        : HeightMap<T>(std::move(heightMap)), resolution_{this->width()} {
        validateDimensions();
    }

    CubeSphere(const CubeSphere&) = default;
    CubeSphere(CubeSphere&&) noexcept = default;
    CubeSphere& operator=(const CubeSphere&) = default;
    CubeSphere& operator=(CubeSphere&&) noexcept = default;

    using HeightMap<T>::at;

    typename std::vector<T>::const_reference at(CubeSphereFace face, std::size_t x, std::size_t y) const {
        return HeightMap<T>::at(x, y + resolution_ * faceID(face));
    }
    typename std::vector<T>::reference at(CubeSphereFace face, std::size_t x, std::size_t y) {
        return HeightMap<T>::at(x, y + resolution_ * faceID(face));
    }
    glm::vec3 pointUnitDir(CubeSphereFace face, std::size_t x, std::size_t y) const {
        const float denominator = static_cast<float>(resolution_ - 1);
        const float u = -1.0F + 2.0F * static_cast<float>(x) / denominator;
        const float v = -1.0F + 2.0F * static_cast<float>(y) / denominator;
        return cubeSphereDirection(face, u, v);
    }

    void setRadius(float radius) {
        validateRadius(radius);
        radius_ = radius;
    }
    float radius() const { return radius_; }
    std::size_t resolution() const { return resolution_; }

private:
    void validateDimensions() const {
        if (resolution_ < 2 || this->height() != stackedHeight(resolution_)) {
            throw std::invalid_argument("cube sphere height map dimensions must be N x 6N");
        }
    }

    static std::size_t stackedHeight(std::size_t resolution) {
        if (resolution > std::numeric_limits<std::size_t>::max() / FACES.size()) {
            throw std::overflow_error("cube sphere stacked height overflows size_t");
        }
        return resolution * FACES.size();
    }

    static void validateRadius(float radius) {
        if (!std::isfinite(radius) || radius <= 0.0F) {
            throw std::invalid_argument("cube sphere radius must be finite and positive");
        }
    }

    std::size_t resolution_{};
    float radius_{100.0F};
};

}
