#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wgen {

glm::vec3 terrainColor(float height);

glm::vec3 terrainBlackAndWhite(float height);

class HeightMap {
public:
    HeightMap(std::size_t width, std::size_t height);

    HeightMap(const HeightMap& heightmap) = default;
    HeightMap(HeightMap&& heightmap) = default;

    HeightMap& operator=(const HeightMap&) = default;
    HeightMap& operator=(HeightMap&&) noexcept = default;

    float &at(std::size_t x, std::size_t y) { return samples_.at(y * width_ + x); }
    float at(std::size_t x, std::size_t y) const { return samples_.at(y * width_ + x); }
    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }

    HeightMap& normalize();
    HeightMap normal() const;

private:
    std::size_t width_;
    std::size_t height_;
    std::vector<float> samples_;
};

} // namespace wgen
