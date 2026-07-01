#pragma once

#include "terrain/generators/generator.hpp"

#include <cstdint>


namespace wgen {

    class ValueNoiseGenerator : public Generator {
    public:
        ValueNoiseGenerator();
        explicit ValueNoiseGenerator(SeedType seed);

        float noise(std::size_t x, std::size_t y) const override;
        GeneratorCapabilities capabilities() const override;
        virtual std::string compShader() const override;
        virtual std::size_t specSize() const override;
    };

}
