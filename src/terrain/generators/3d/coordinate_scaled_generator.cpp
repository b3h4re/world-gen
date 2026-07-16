#include "coordinate_scaled_generator.hpp"

#include <stdexcept>
#include <utility>

namespace wgen {

CoordinateScaledGenerator3d::CoordinateScaledGenerator3d(
        std::unique_ptr<Generator3d> generator,
        float coordinateScale)
    : generator_{std::move(generator)}, coordinateScale_{coordinateScale} {
    if (generator_ == nullptr) {
        throw std::invalid_argument("3D coordinate-scaled generator cannot wrap null generator");
    }
    if (coordinateScale_ <= 0.0F) {
        throw std::invalid_argument("3D coordinate scale must be positive");
    }
}

void CoordinateScaledGenerator3d::setSeed(SeedType newSeed) {
    Generator3d::setSeed(newSeed);
    generator_->setSeed(newSeed);
}

GeneratorCapabilities CoordinateScaledGenerator3d::capabilities() const {
    return generator_->capabilities();
}

float CoordinateScaledGenerator3d::noise(glm::dvec3 point) const {
    return generator_->noise(point * static_cast<double>(coordinateScale_));
}

} // namespace wgen
