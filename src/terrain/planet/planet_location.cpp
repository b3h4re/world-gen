#include "planet_location.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace wgen {
namespace {

bool isFinite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

void validateOrientation(const PlanetOrientation& orientation) {
    if (!std::isfinite(orientation.headingRadians) ||
            !std::isfinite(orientation.pitchRadians) ||
            !std::isfinite(orientation.rollRadians)) {
        throw std::invalid_argument{"planet orientation must be finite"};
    }
}

void validateRadius(double planetRadiusMeters) {
    if (!std::isfinite(planetRadiusMeters) || planetRadiusMeters <= 0.0) {
        throw std::invalid_argument{"planet radius must be finite and positive"};
    }
}

} // namespace

void validatePlanetLocation(const PlanetLocation& location) {
    if (!isFinite(location.unitSurfaceDirection) ||
            std::abs(glm::length(location.unitSurfaceDirection) - 1.0) >
                0.000000000001 ||
            !std::isfinite(location.altitudeMeters)) {
        throw std::invalid_argument{
            "planet location direction must be unit length and coordinates must be finite"};
    }
    validateOrientation(location.orientation);
    if (location.faceUv) {
        const glm::dvec3 cachedDirection = cubeSphereDirection(
            location.faceUv->face,
            location.faceUv->u,
            location.faceUv->v);
        if (glm::length(cachedDirection - location.unitSurfaceDirection) >
                0.000000001) {
            throw std::invalid_argument{
                "planet location face/UV cache disagrees with its direction"};
        }
    }
}

PlanetLocation makePlanetLocation(
        glm::dvec3 surfaceDirection,
        double altitudeMeters,
        PlanetOrientation orientation,
        std::optional<CubeSphereFace> preferredFace) {
    if (!isFinite(surfaceDirection) ||
            glm::length(surfaceDirection) <=
                std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument{
            "planet location surface direction must be finite and non-zero"};
    }
    if (!std::isfinite(altitudeMeters)) {
        throw std::invalid_argument{"planet location altitude must be finite"};
    }
    validateOrientation(orientation);

    PlanetLocation result{
        .unitSurfaceDirection = glm::normalize(surfaceDirection),
        .altitudeMeters = altitudeMeters,
        .orientation = orientation,
    };
    result.faceUv = directionToCubeSphereFaceUv(
        result.unitSurfaceDirection,
        preferredFace);
    validatePlanetLocation(result);
    return result;
}

PlanetLocation reanchorPlanetLocation(
        const PlanetLocation& location,
        glm::dvec3 surfaceDirection,
        std::optional<CubeSphereFace> preferredFace) {
    validatePlanetLocation(location);
    return makePlanetLocation(
        surfaceDirection,
        location.altitudeMeters,
        location.orientation,
        preferredFace);
}

PlanetTangentFrame planetTangentFrame(glm::dvec3 surfaceDirection) {
    if (!isFinite(surfaceDirection) ||
            glm::length(surfaceDirection) <=
                std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument{
            "planet tangent frame direction must be finite and non-zero"};
    }
    const glm::dvec3 up = glm::normalize(surfaceDirection);
    const glm::dvec3 reference = std::abs(up.z) < 0.999
        ? glm::dvec3{0.0, 0.0, 1.0}
        : glm::dvec3{0.0, 1.0, 0.0};
    const glm::dvec3 north = glm::normalize(
        reference - up * glm::dot(reference, up));
    const glm::dvec3 east = glm::normalize(glm::cross(north, up));
    return {east, north, up};
}

glm::dvec3 planetLocationPositionMeters(
        const PlanetLocation& location,
        double planetRadiusMeters) {
    validatePlanetLocation(location);
    validateRadius(planetRadiusMeters);
    const double radialDistance = planetRadiusMeters + location.altitudeMeters;
    if (!std::isfinite(radialDistance) || radialDistance <= 0.0) {
        throw std::invalid_argument{
            "planet location altitude must remain above the planet center"};
    }
    return location.unitSurfaceDirection * radialDistance;
}

glm::dvec3 planetLocationPositionInPlanetRadii(
        const PlanetLocation& location,
        double planetRadiusMeters) {
    return planetLocationPositionMeters(location, planetRadiusMeters) /
        planetRadiusMeters;
}

} // namespace wgen
