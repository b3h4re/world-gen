#include "terrain.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <stdexcept>

namespace wgen {

/*
So first map [-1; 1] to half circle in complex plain
        -1 |---> -i (blue)
         0 |--->  1 (green)
         1 |--->  i (red)
Which is w(x) = (i - x) / (i + x)
So we get a vector in 2d space with
Re(w) = (1 - x^2) / (1 + x^2)
Im(w) = 2 x / (1 + x^2)

Then convert the resulting vector to HSV and then to RGB
*/
glm::vec3 terrainColor(float height) {
    if (std::abs(height) > 1.0f) {
        std::runtime_error("height must be in [-1; 1] to get color mapping.");
    }

    float re = (1 - height * height) / (1 + height * height);
    float im = 2 * height / (1 + height * height);

    float theta = std::atan2(im, re);

    // convert to HSV first, S = 1, V = 1
    // H' = 6H
    float H_ = 6 * (0.333f - 2 * theta * 0.333f / glm::pi<float>());

    // temp value for conversion
    float X = 1 - std::abs(glm::mod<float>(H_, 2) - 1);

    if (H_ < 1) {
        return glm::vec3{1.0f, X, 0.0f};
    }

    if (H_ < 2) {
        return glm::vec3{X, 1.0f, 0.0f};
    }

    if (H_ < 3) {
        return glm::vec3{0.0f, 1.0f, X};
    }

    if (H_ < 4) {
        return glm::vec3{0.0f, X, 1.0f};
    }

    if (H_ < 5) {
        return glm::vec3{X, 0.0f, 1.0f};
    }

    return glm::vec3{1.0f, 0.0f, X};

}


glm::vec3 terrainBlackAndWhite(float height) {
    if (std::abs(height) > 1.0f) {
        std::runtime_error("height must be in [-1; 1] to get color mapping.");
    }

    return glm::vec3{(height + 1) / 2};
}

HeightMap::HeightMap(std::size_t width, std::size_t height) : width_{width}, height_{height}, samples_(width * height) {
    if (width < 2 || height < 2) {
        throw std::invalid_argument("height map dimensions must be at least 2x2");
    }
}

HeightMap& HeightMap::normalize() {
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
            maxAbs = std::max(maxAbs, std::abs(this->at(x, y)));
        }
    }

    for (std::size_t y = 0; y < height_; ++y) {
        for (std::size_t x = 0; x < width_; ++x) {
            this->at(x, y) /= maxAbs;
        }
    }
    return *this;
}

HeightMap HeightMap::normal() const {
    HeightMap newHeightmap{*this};
    newHeightmap.normalize();
    return newHeightmap;
}

}
