#pragma once

#include "terrain/terrain.hpp"

#include <cstddef>

#include <glm/gtc/constants.hpp>

namespace wgen {

/*
Planet accepts its Radius (float R) and heightmap min and heightmap max (float hMax, float hMin).
All the terrain on the planet is in [R - hMax, R + hMin].
Gneerator uses 3d terrain gen for each point (x, y, z) on the sphere to get noise.

Points on the sphere are determined by spherical coordinates parametrisation. Each angle step is 2*pi / dots (size_t dots).

x = R * cos(phi) * cos(psi)
y = R * sin(phi) * cos(psi)
z = R * sin(psi)

Planet extends heightmap where at(x, y) method translates to the point with phi = 2 * pi * x / dots; psi = 2 * pi * y / dots


Also provide a method to get (float x, float y, float z) on the sphere from (size_t x, size_t y).

Basically the only difference between a Planet and a regular heightMap<float> is that a planet maps its values.
*/

template <class T>
class Planet : public HeightMap<T> {
public:
    Planet() : HeightMap<T>(100, 100), radius_{100.0f}, dots_{100} {}

    Planet(std::size_t dots) : HeightMap<T>(dots, dots), radius_{100.0f}, dots_{dots} {}

    Planet(std::size_t dots, const T& defaultValue)
        : HeightMap<T>(dots, dots, defaultValue), radius_{100.0f}, dots_{dots} {}

    Planet(const HeightMap<T>& h) : HeightMap<T>(h), dots_{h.width()}, radius_{100.0f} {
        assert(h.width() == h.height() && "To init a planet from heightmap it must have equal width and height");
    }

    Planet(HeightMap<T>&& h) : HeightMap<T>(h), dots_{h.width()}, radius_{100.0f} {
        assert(h.width() == h.height() && "To init a planet from heightmap it must have equal width and height");
    }


    Planet(std::initializer_list<std::initializer_list<T>> rows)
        : HeightMap<T>(rows), radius_{100.0f}, dots_{rows.size()} {
        if (this->width() != this->height()) {
            throw std::invalid_argument("height and width for a planet must be equal");
        }
    }

    Planet(const Planet& planet) = default;
    Planet(Planet&& planet) = default;

    Planet& operator=(const Planet&) = default;
    Planet& operator=(Planet&&) noexcept = default;

    glm::vec3 pointPosition(std::size_t x, std::size_t y) const {
        const float phi = 2 * glm::pi<float>() * static_cast<float>(x) / static_cast<float>(dots_);
        const float psi = 2 * glm::pi<float>() * static_cast<float>(y) / static_cast<float>(dots_);

        glm::vec3 pos{
            radius_ * glm::cos(phi) * glm::cos(psi),
            radius_ * glm::sin(phi) * glm::cos(psi),
            radius_ * glm::sin(psi)
        };
        return pos;
    }

    glm::vec3 pointUnitDir(std::size_t x, std::size_t y) const {
        glm::vec3 pos = pointPosition(x, y);
        return glm::normalize(pos);
    }

    void setRadius(float R) { radius_ = R; }
    float radius() const { return radius_; }

private:
    float radius_;
    std::size_t dots_;
};


}
