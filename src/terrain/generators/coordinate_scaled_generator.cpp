#include "coordinate_scaled_generator.hpp"

#include <stdexcept>
#include <utility>

namespace wgen {

CoordinateScaledGenerator::CoordinateScaledGenerator(std::unique_ptr<Generator> generator, float coordinateScale)
    : generator_{std::move(generator)}, coordinateScale_{coordinateScale} {
    if (generator_ == nullptr) {
        throw std::invalid_argument("coordinate-scaled generator cannot wrap null generator");
    }
    if (coordinateScale_ <= 0.0F) {
        throw std::invalid_argument("coordinate scale must be positive");
    }
}

void CoordinateScaledGenerator::setSeed(SeedType newSeed) {
    Generator::setSeed(newSeed);
    generator_->setSeed(newSeed);
}

GeneratorCapabilities CoordinateScaledGenerator::capabilities() const {
    return generator_->capabilities();
}

float CoordinateScaledGenerator::noise(std::size_t x, std::size_t y) const {
    return generator_->noise(scaleCoordinate(x), scaleCoordinate(y));
}

std::size_t CoordinateScaledGenerator::scaleCoordinate(std::size_t coordinate) const {
    return static_cast<std::size_t>(static_cast<float>(coordinate) * coordinateScale_);
}

} // namespace wgen
