#pragma once

#include "terrain/planet/planet_location.hpp"

#include <cstdint>

#include <glm/glm.hpp>

namespace wgen {

// Double-precision reference frame shared by local navigation, clipmap
// sampling, and later camera-relative rendering. The anchor position lies on
// the undisplaced reference sphere; terrain height is applied per sample.
struct LocalPlanetFrame {
    glm::dvec3 anchorDirection{};
    glm::dvec3 east{};
    glm::dvec3 north{};
    glm::dvec3 up{};
    double planetRadiusMeters{};
    glm::dvec3 globalAnchorPositionMeters{};
    std::uint64_t revision{};
};

void validateLocalPlanetFrame(const LocalPlanetFrame& frame);

LocalPlanetFrame makeLocalPlanetFrame(
    glm::dvec3 anchorDirection,
    double planetRadiusMeters,
    std::uint64_t revision);

LocalPlanetFrame makeLocalPlanetFrame(
    const PlanetTangentFrame& basis,
    double planetRadiusMeters,
    std::uint64_t revision);

// Exponential-map-style projection from tangent-plane meters to a direction
// on the reference sphere. It is independent of cube-face ownership.
glm::dvec3 localPlanetSurfaceDirection(
    const LocalPlanetFrame& frame,
    glm::dvec2 localPositionMeters);

// Inverse of localPlanetSurfaceDirection for every direction except the
// anchor's ambiguous antipode. This is also the canonical coordinate mapping
// used by hybrid quadtree/clipmap coverage.
glm::dvec2 localPlanetTangentPosition(
    const LocalPlanetFrame& frame,
    glm::dvec3 surfaceDirection);

glm::dvec3 localPlanetCurvedPosition(
    const LocalPlanetFrame& frame,
    glm::dvec3 surfaceDirection,
    double heightMeters);

glm::dvec3 localPlanetTangentFlatPosition(
    const LocalPlanetFrame& frame,
    glm::dvec2 localPositionMeters,
    double heightMeters);

} // namespace wgen
