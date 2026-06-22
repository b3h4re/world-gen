#pragma once

#include "terrain/generators/generator.hpp"



namespace wgen {

    class ValueNoiseGenerator : public Generator {
    public:
        ValueNoiseGenerator();
        explicit ValueNoiseGenerator(std::uint32_t seed);

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) override;
    };

}
