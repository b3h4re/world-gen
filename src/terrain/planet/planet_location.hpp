#pragma once

#include "terrain/planet/cube_sphere.hpp"

#include <optional>

#include <glm/glm.hpp>

namespace wgen {

struct PlanetOrientation {
    double headingRadians{};
    double pitchRadians{};
    double rollRadians{};

    bool operator==(const PlanetOrientation&) const = default;
};

struct PlanetLocation {
    // This is global navigation state. It intentionally contains no render
    // origin or camera-relative coordinate; altitude is always physical.
    glm::dvec3 unitSurfaceDirection{0.0, 0.0, 1.0};
    double altitudeMeters{};
    std::optional<CubeSphereFaceUv> faceUv{};
    PlanetOrientation orientation{};

    bool operator==(const PlanetLocation&) const = default;
};

struct PlanetTangentFrame {
    glm::dvec3 east{};
    glm::dvec3 north{};
    glm::dvec3 up{};
};

void validatePlanetLocation(const PlanetLocation& location);

PlanetLocation makePlanetLocation(
    glm::dvec3 surfaceDirection,
    double altitudeMeters,
    PlanetOrientation orientation = {},
    std::optional<CubeSphereFace> preferredFace = std::nullopt);

PlanetLocation reanchorPlanetLocation(
    const PlanetLocation& location,
    glm::dvec3 surfaceDirection,
    std::optional<CubeSphereFace> preferredFace = std::nullopt);

PlanetTangentFrame planetTangentFrame(glm::dvec3 surfaceDirection);

glm::dvec3 planetLocationPositionMeters(
    const PlanetLocation& location,
    double planetRadiusMeters);

glm::dvec3 planetLocationPositionInPlanetRadii(
    const PlanetLocation& location,
    double planetRadiusMeters);

} // namespace wgen
