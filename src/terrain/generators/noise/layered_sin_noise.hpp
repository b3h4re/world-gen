#pragma once

#include "terrain/generators/generator.hpp"



namespace wgen {

    class LayeredSinNoiseGenerator : public Generator {
    public:
        LayeredSinNoiseGenerator();
        explicit LayeredSinNoiseGenerator(std::uint32_t seed);

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) override;
    };

}
