#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace wgen {

glm::vec3 terrainColor(float height);

glm::vec3 terrainBlackAndWhite(float height);

template<class T>
class HeightMap {
public:
    HeightMap() : width_{0}, height_{0}, samples_{0} {}
    HeightMap(std::size_t width, std::size_t height) : width_{width}, height_{height}, samples_(width * height) {
        if (width < 2 || height < 2) {
            throw std::invalid_argument("height map dimensions must be at least 2x2");
        }
    }

    HeightMap(const HeightMap& heightmap) = default;
    HeightMap(HeightMap&& heightmap) = default;

    HeightMap& operator=(const HeightMap&) = default;
    HeightMap& operator=(HeightMap&&) noexcept = default;

    T &at(std::size_t x, std::size_t y) { return samples_.at(y * width_ + x); }
    T at(std::size_t x, std::size_t y) const { return samples_.at(y * width_ + x); }
    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }

    HeightMap& normalize() {
        float avg{0};
        float size = width_ * height_;

        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                avg += this->at(x, y) / size;
            }
        }

        float maxAbs{0};

        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                this->at(x, y) -= avg;
                maxAbs = std::max(maxAbs, glm::abs(this->at(x, y)));
            }
        }

        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                this->at(x, y) /= maxAbs;
            }
        }
        return *this;
    }
    HeightMap normal() const {
        HeightMap newHeightmap{*this};
        newHeightmap.normalize();
        return newHeightmap;
    }

private:
    std::size_t width_;
    std::size_t height_;
    std::vector<T> samples_;
};

} // namespace wgen
