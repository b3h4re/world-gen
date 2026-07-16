#pragma once

#include "terrain/generators/3d/generator.hpp"

#include <cstddef>

namespace wgen {

class PerlinNoise3d : public Generator3d {
public:
    using FloatFunction = float (*)(float);

    PerlinNoise3d(SeedType seed, float cellSize, FloatFunction funcInterpolate = defaultPerlinInterp);

    using Generator3d::noise;
    float noise(glm::dvec3 point) const override;
    GeneratorCapabilities capabilities() const override;
    std::string compShader() const override;
    std::size_t specSize() const override;

private:
    double cellSize_;
    FloatFunction funcInterpolate_;
};

} // namespace wgen
