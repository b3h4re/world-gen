#pragma once

#include "terrain/generators/generator.hpp"

#include <cstdint>


namespace wgen {

    class LayeredSinNoiseGenerator : public Generator {
    public:
        LayeredSinNoiseGenerator();
        explicit LayeredSinNoiseGenerator(SeedType seed);

        float noise(std::size_t x, std::size_t y) const override;
    };

}
