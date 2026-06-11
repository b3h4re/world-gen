#pragma once

#include "terrain/terrain.hpp"

namespace wgen {

    class Generator {
    public:
        virtual HeightMap<float> generateheightMap(std::size_t width, std::size_t height) = 0;

        virtual void setSeed(const std::uint32_t& newSeed) { seed = newSeed; }
        virtual std::uint32_t getSeed() const { return seed; }
    private:
        std::uint32_t seed;
    };

}
