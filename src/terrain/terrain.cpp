#include "terrain.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace wgen {

HeightMap::HeightMap(std::size_t width, std::size_t height) : width_{width}, height_{height}, samples_(width * height) {
    if (width < 2 || height < 2) {
        throw std::invalid_argument("height map dimensions must be at least 2x2");
    }
}

}
