#pragma once

#include "terrain/generators/3d/generator.hpp"

#include <memory>

namespace wgen {

class CoordinateScaledGenerator3d final : public Generator3d {
public:
    CoordinateScaledGenerator3d(
        std::unique_ptr<Generator3d> generator,
        float coordinateScale);

    void setSeed(SeedType newSeed) override;
    GeneratorCapabilities capabilities() const override;
    using Generator3d::noise;
    float noise(glm::dvec3 point) const override;

private:
    std::unique_ptr<Generator3d> generator_;
    float coordinateScale_{1.0F};
};

} // namespace wgen
