#include "terrain/planet/local_planet_frame.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace wgen {
namespace {

constexpr double VECTOR_EPSILON = 1.0e-15;
constexpr double FRAME_EPSILON = 1.0e-10;

bool isFinite(const glm::dvec2& value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool isFinite(const glm::dvec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

void validateHeight(double heightMeters) {
    if (!std::isfinite(heightMeters)) {
        throw std::invalid_argument{"local planet height must be finite"};
    }
}

} // namespace

void validateLocalPlanetFrame(const LocalPlanetFrame& frame) {
    if (!isFinite(frame.anchorDirection) || !isFinite(frame.east) ||
            !isFinite(frame.north) || !isFinite(frame.up) ||
            !isFinite(frame.globalAnchorPositionMeters) ||
            !std::isfinite(frame.planetRadiusMeters) ||
            frame.planetRadiusMeters <= 0.0 || frame.revision == 0) {
        throw std::invalid_argument{"local planet frame contains invalid values"};
    }
    if (std::abs(glm::length(frame.anchorDirection) - 1.0) > FRAME_EPSILON ||
            std::abs(glm::length(frame.east) - 1.0) > FRAME_EPSILON ||
            std::abs(glm::length(frame.north) - 1.0) > FRAME_EPSILON ||
            std::abs(glm::length(frame.up) - 1.0) > FRAME_EPSILON ||
            glm::length(frame.anchorDirection - frame.up) > FRAME_EPSILON ||
            std::abs(glm::dot(frame.east, frame.north)) > FRAME_EPSILON ||
            std::abs(glm::dot(frame.east, frame.up)) > FRAME_EPSILON ||
            std::abs(glm::dot(frame.north, frame.up)) > FRAME_EPSILON ||
            glm::length(glm::cross(frame.east, frame.north) - frame.up) >
                FRAME_EPSILON) {
        throw std::invalid_argument{
            "local planet frame basis must be right-handed and orthonormal"};
    }

    const glm::dvec3 expectedAnchor =
        frame.anchorDirection * frame.planetRadiusMeters;
    const double anchorTolerance = std::max(
        1.0e-9,
        frame.planetRadiusMeters * 1.0e-12);
    if (glm::length(frame.globalAnchorPositionMeters - expectedAnchor) >
            anchorTolerance) {
        throw std::invalid_argument{
            "local planet global anchor must lie on the reference sphere"};
    }
}

LocalPlanetFrame makeLocalPlanetFrame(
        glm::dvec3 anchorDirection,
        double planetRadiusMeters,
        std::uint64_t revision) {
    if (!isFinite(anchorDirection) ||
            glm::length(anchorDirection) <=
                std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument{
            "local planet anchor direction must be finite and non-zero"};
    }
    return makeLocalPlanetFrame(
        planetTangentFrame(anchorDirection),
        planetRadiusMeters,
        revision);
}

LocalPlanetFrame makeLocalPlanetFrame(
        const PlanetTangentFrame& basis,
        double planetRadiusMeters,
        std::uint64_t revision) {
    LocalPlanetFrame frame{
        .anchorDirection = basis.up,
        .east = basis.east,
        .north = basis.north,
        .up = basis.up,
        .planetRadiusMeters = planetRadiusMeters,
        .globalAnchorPositionMeters = basis.up * planetRadiusMeters,
        .revision = revision,
    };
    validateLocalPlanetFrame(frame);
    return frame;
}

glm::dvec3 localPlanetSurfaceDirection(
        const LocalPlanetFrame& frame,
        glm::dvec2 localPositionMeters) {
    validateLocalPlanetFrame(frame);
    if (!isFinite(localPositionMeters)) {
        throw std::invalid_argument{
            "local planet tangent position must be finite"};
    }

    const double distance = glm::length(localPositionMeters);
    if (distance <= VECTOR_EPSILON) {
        return frame.anchorDirection;
    }
    const double angle = distance / frame.planetRadiusMeters;
    if (!std::isfinite(angle) || angle >= std::numbers::pi) {
        throw std::invalid_argument{
            "local planet tangent position reaches an ambiguous antipode"};
    }
    const glm::dvec3 tangent = glm::normalize(
        frame.east * localPositionMeters.x +
        frame.north * localPositionMeters.y);
    return glm::normalize(
        frame.anchorDirection * std::cos(angle) +
        tangent * std::sin(angle));
}

glm::dvec3 localPlanetCurvedPosition(
        const LocalPlanetFrame& frame,
        glm::dvec3 surfaceDirection,
        double heightMeters) {
    validateLocalPlanetFrame(frame);
    validateHeight(heightMeters);
    if (!isFinite(surfaceDirection) ||
            glm::length(surfaceDirection) <= VECTOR_EPSILON) {
        throw std::invalid_argument{
            "local planet surface direction must be finite and non-zero"};
    }
    const double radialDistance = frame.planetRadiusMeters + heightMeters;
    if (!std::isfinite(radialDistance) || radialDistance <= 0.0) {
        throw std::invalid_argument{
            "local planet height must remain outside the planet center"};
    }
    return glm::normalize(surfaceDirection) * radialDistance;
}

glm::dvec3 localPlanetTangentFlatPosition(
        const LocalPlanetFrame& frame,
        glm::dvec2 localPositionMeters,
        double heightMeters) {
    validateLocalPlanetFrame(frame);
    validateHeight(heightMeters);
    if (!isFinite(localPositionMeters)) {
        throw std::invalid_argument{
            "local planet tangent position must be finite"};
    }
    const glm::dvec3 position = frame.globalAnchorPositionMeters +
        frame.east * localPositionMeters.x +
        frame.north * localPositionMeters.y +
        frame.up * heightMeters;
    if (!isFinite(position)) {
        throw std::overflow_error{"local planet flat position overflow"};
    }
    return position;
}

} // namespace wgen
