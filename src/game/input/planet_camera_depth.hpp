#pragma once

#include <cstdint>

namespace lve {

enum class PlanetCameraDepthRegime : std::uint8_t {
    Local,
    Transition,
    Orbital,
};

struct PlanetCameraDepthRange {
    float nearPlane{};
    float farPlane{};
    PlanetCameraDepthRegime regime{PlanetCameraDepthRegime::Orbital};
};

// Returns projection distances in render-space planet radii. Local views use
// a horizon-scale far plane while orbital views retain whole-planet coverage.
PlanetCameraDepthRange planetCameraDepthRange(
    double altitudeMeters,
    double planetRadiusMeters);

} // namespace lve
