#pragma once

#include "terrain/terrain.hpp"

namespace wgen {

    class Generator {
    public:
        virtual ~Generator() = default;

        virtual HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) = 0;

        virtual void setSeed(std::uint32_t newSeed) { seed_ = newSeed; }
        std::uint32_t getSeed() const { return seed_; }

    private:
        std::uint32_t seed_{};
    };

}
