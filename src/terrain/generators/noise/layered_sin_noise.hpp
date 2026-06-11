#pragma once

#include "terrain/generators/generator.hpp"



namespace wgen {
    class LayeredSinNoiseGenerator : public Generator {
    public:
        LayeredSinNoiseGenerator();
        LayeredSinNoiseGenerator(std::uint32_t seed);

        HeightMap<float> generateheightMap(std::size_t width, std::size_t height);
    private:
        std::uint32_t seed;
    };
}
