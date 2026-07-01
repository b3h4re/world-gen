#pragma once

#include "terrain/generators/generator.hpp"

#include <cstdint>


namespace wgen {

    class LayeredSinNoiseGenerator : public Generator {
    public:
        LayeredSinNoiseGenerator();
        explicit LayeredSinNoiseGenerator(std::uint32_t seed);

        float noise(std::size_t x, std::size_t y) const override;
    };

}
