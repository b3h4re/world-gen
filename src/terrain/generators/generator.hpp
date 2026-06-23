#pragma once

#include "terrain/terrain.hpp"

namespace wgen {

    class Generator {
    public:
        virtual ~Generator() = default;

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const {
            HeightMap<float> map{width, height};
            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    map.at(x, y) = noise(x, y);
                }
            }

            return map;
        }

        virtual float noise(std::size_t x, std::size_t y) const = 0;

        virtual void setSeed(std::uint32_t newSeed) { seed_ = newSeed; }
        std::uint32_t getSeed() const { return seed_; }

    private:
        std::uint32_t seed_{};
    };

}
