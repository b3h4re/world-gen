#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wgen {

struct WorldBlock {
};

class HeightMap {
public:
    HeightMap(std::size_t width, std::size_t height);

    float &at(std::size_t x, std::size_t y) { return samples_.at(y * width_ + x); }
    float at(std::size_t x, std::size_t y) const { return samples_.at(y * width_ + x); }
    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }

private:
    std::size_t width_;
    std::size_t height_;
    std::vector<float> samples_;
};

} // namespace wgen
