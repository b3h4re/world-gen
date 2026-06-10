#pragma once

#include "terrain/terrain.hpp"

namespace wgen {

    class Generator {
    public:
        virtual HeightMap generateheightMap(std::size_t width, std::size_t height) = 0;
    private:
        std::uint32_t seed;
    };

}
