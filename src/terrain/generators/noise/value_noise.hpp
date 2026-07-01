#pragma once

#include "terrain/generators/generator.hpp"

#include <cstdint>


namespace wgen {

    class ValueNoiseGenerator : public Generator {
    public:
        ValueNoiseGenerator();
        explicit ValueNoiseGenerator(std::uint32_t seed);

        float noise(std::size_t x, std::size_t y) const override;
        GeneratorCapabilities capabilities() const override;
        GeneratorSpec spec() const override;
    };

}
