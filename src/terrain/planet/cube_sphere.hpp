#pragma once

#include "terrain/terrain.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <array>



namespace wgen {

enum class CubeSphereFace {
    Top,     // +Z
    Bottom,  // -Z
    Right,   // +X
    Left,    // -X
    Back,    // -Y
    Front    // +Y
};

constexpr CubeSphereFace FACES[6] = {
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


/*
So basically use a heightmap to store 6 faces of a cube.
Internally they are just 6 stacked heightmaps of size resolution * resolution
*/
template <class T>
class CubeSphere : HeightMap<T> {
public:
    CubeSphere() : HeightMap<T>(), radius_{10.0f} {}
    CubeSphere(std::size_t resolution) : HeightMap<T>(resolution, resolution * 6), radius_{10.0f} {}
    CubeSphere(float R, std::size_t resolution) : HeightMap<T>(resolution, resolution * 6), radius_{R} {}
    CubeSphere(std::size_t resolution, const T& defaultValue)
                : HeightMap<T>(resolution, resolution * 6, defaultValue), radius_{10.0f} {}
    CubeSphere(float R, std::size_t resolution, const T& defaultValue)
                : HeightMap<T>(resolution, resolution * 6, defaultValue), radius_{R} {}

    CubeSphere(const HeightMap<T>& h) {
        resolution_ = std::min(h.width(), h.height());
        for (const auto f : wgen::FACES) {
            for (std::size_t x = 0; x < resolution_; ++x) {
                for (std::size_t y = 0; y < resolution_; ++y) {
                    this->at(f, x, y) = h.at(x, y);
                }
            }
        }
    }


    typename std::vector<T>::const_reference at(CubeSphereFace face, std::size_t x, std::size_t y) const {
        return HeightMap<T>::at(x, y + resolution_ * faceID(face));
    }
    typename std::vector<T>::reference at(CubeSphereFace face, std::size_t x, std::size_t y) {
        return HeightMap<T>::at(x, y + resolution_ * faceID(face));
    }
    glm::vec3 pointUnitDir(CubeSphereFace face, std::size_t x, std::size_t y) const {
        const float u = static_cast<float>(x) / static_cast<float>(resolution_);
        const float v = static_cast<float>(y) / static_cast<float>(resolution_);
        return cubeSphereDirection(face, u, v);
    }

    float radius() const { return radius_; }
    float resolution() const { return resolution_; }



private:
    std::size_t resolution_;
    float radius_;

};

}
