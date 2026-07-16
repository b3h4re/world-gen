#include "planet_camera_depth.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lve {

PlanetCameraDepthRange planetCameraDepthRange(
        double altitudeMeters,
        double planetRadiusMeters) {
    if (!std::isfinite(altitudeMeters) || altitudeMeters < 0.0 ||
            !std::isfinite(planetRadiusMeters) || planetRadiusMeters <= 0.0) {
        throw std::invalid_argument{"planet camera depth parameters are invalid"};
    }

    constexpr double LOCAL_LIMIT_RADII = 0.02;
    constexpr double ORBITAL_LIMIT_RADII = 0.1;
    constexpr double MINIMUM_ALTITUDE_RADII = 1.0e-9;

    const double altitude = std::max(
        altitudeMeters / planetRadiusMeters,
        MINIMUM_ALTITUDE_RADII);
    const double horizonDistance = std::sqrt(altitude * (2.0 + altitude));
    const double localNear = std::max(1.0e-8, altitude * 0.02);
    const double localFar = std::max(localNear * 32.0, horizonDistance * 2.5);
    const double orbitalNear = std::clamp(altitude * 0.02, 0.001, 0.1);
    // Looking toward the center, the far side is altitude + two radii away.
    const double orbitalFar = altitude + 2.25;

    if (altitude <= LOCAL_LIMIT_RADII) {
        return {
            .nearPlane = static_cast<float>(localNear),
            .farPlane = static_cast<float>(localFar),
            .regime = PlanetCameraDepthRegime::Local,
        };
    }
    if (altitude >= ORBITAL_LIMIT_RADII) {
        return {
            .nearPlane = static_cast<float>(orbitalNear),
            .farPlane = static_cast<float>(orbitalFar),
            .regime = PlanetCameraDepthRegime::Orbital,
        };
    }

    const double unscaledWeight =
        (altitude - LOCAL_LIMIT_RADII) /
        (ORBITAL_LIMIT_RADII - LOCAL_LIMIT_RADII);
    const double weight = unscaledWeight * unscaledWeight *
        (3.0 - 2.0 * unscaledWeight);
    return {
        .nearPlane = static_cast<float>(std::lerp(localNear, orbitalNear, weight)),
        .farPlane = static_cast<float>(std::lerp(localFar, orbitalFar, weight)),
        .regime = PlanetCameraDepthRegime::Transition,
    };
}

} // namespace lve
