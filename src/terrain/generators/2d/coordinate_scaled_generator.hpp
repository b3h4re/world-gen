#pragma once

#include "terrain/generators/2d/generator.hpp"

#include <cstddef>
#include <memory>

namespace wgen {

class CoordinateScaledGenerator final : public Generator {
public:
    CoordinateScaledGenerator(
        std::unique_ptr<Generator> generator,
        float coordinateScale);

    void setSeed(SeedType newSeed) override;
    GeneratorCapabilities capabilities() const override;
    float noise(std::size_t x, std::size_t y) const override;

private:
    std::size_t scaleCoordinate(std::size_t coordinate) const;

    std::unique_ptr<Generator> generator_;
    float coordinateScale_{1.0F};
};

} // namespace wgen
