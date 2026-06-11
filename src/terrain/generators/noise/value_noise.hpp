#pragma once

#include "terrain/generators/generator.hpp"



namespace wgen {
    class ValueNoiseGenerator : public Generator {
    public:
        ValueNoiseGenerator();
        ValueNoiseGenerator(std::uint32_t seed);

        HeightMap<float> generateheightMap(std::size_t width, std::size_t height);
    private:
        std::uint32_t seed;
    };
}
