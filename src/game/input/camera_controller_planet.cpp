#include "camera_controller_planet.hpp"

#include <iostream>


#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace lve {

void CameraControllerPlanet::update(
        const AppInputState& input,
        float frameTime,
        double planetRadiusMeters,
        Camera3d& camera) {
    if (!std::isfinite(frameTime) || frameTime < 0.0F ||
            !std::isfinite(planetRadiusMeters) || planetRadiusMeters <= 0.0) {
        throw std::invalid_argument{"planet camera update parameters are invalid"};
    }
    constexpr double ROTATE_SPEED = 1.5;
    constexpr double ZOOM_SPEED_RADII = 2.0;

    if (!locationInitialized_) {
        location_ = wgen::makePlanetLocation(
            {0.0, 1.0, 0.0},
            planetRadiusMeters,
            {.pitchRadians = -std::numbers::pi / 2.0});
        locationInitialized_ = true;
    }

    double deltaYaw{};
    double deltaPitch{};
    deltaYaw += input.cameraMoveRight ? ROTATE_SPEED * frameTime : 0.0;
    deltaYaw -= input.cameraMoveLeft ? ROTATE_SPEED * frameTime : 0.0;
    deltaPitch += input.cameraMoveUp ? ROTATE_SPEED * frameTime : 0.0;
    deltaPitch -= input.cameraMoveDown ? ROTATE_SPEED * frameTime : 0.0;
    location_.altitudeMeters += input.cameraZoomOut
        ? ZOOM_SPEED_RADII * planetRadiusMeters * frameTime
        : 0.0;
    location_.altitudeMeters -= input.cameraZoomIn
        ? ZOOM_SPEED_RADII * planetRadiusMeters * frameTime
        : 0.0;
    location_.altitudeMeters = std::clamp(
        location_.altitudeMeters,
        0.2 * planetRadiusMeters,
        7.0 * planetRadiusMeters);

    const glm::dquat rotateUp{
        glm::cos(deltaPitch / 2.0),
        0.0,
        glm::sin(deltaPitch / 2.0),
        0.0,
    };
    const glm::dquat rotateSide{
        glm::cos(deltaYaw / 2.0),
        glm::sin(deltaYaw / 2.0),
        0.0,
        0.0,
    };

    orbitRotation_ = glm::normalize(
        orbitRotation_ * rotateSide * rotateUp);

    const glm::dquat positionQuaternion = orbitRotation_ *
        glm::dquat{0.0, startingPosition} * glm::inverse(orbitRotation_);
    const glm::dquat upQuaternion = orbitRotation_ *
        glm::dquat{0.0, startingUp} * glm::inverse(orbitRotation_);

    const glm::dvec3 surfaceDirection = glm::normalize(glm::dvec3{
        positionQuaternion.y,
        positionQuaternion.z,
        positionQuaternion.x,
    });
    const glm::dvec3 cameraUp = glm::normalize(glm::dvec3{
        upQuaternion.y,
        upQuaternion.z,
        upQuaternion.x,
    });
    const wgen::PlanetTangentFrame tangentFrame =
        wgen::planetTangentFrame(surfaceDirection);
    location_.unitSurfaceDirection = surfaceDirection;
    location_.faceUv = wgen::directionToCubeSphereFaceUv(
        surfaceDirection,
        location_.faceUv
            ? std::optional{location_.faceUv->face}
            : std::nullopt);
    location_.orientation.headingRadians = std::atan2(
        glm::dot(cameraUp, tangentFrame.east),
        glm::dot(cameraUp, tangentFrame.north));
    location_.orientation.pitchRadians = -std::numbers::pi / 2.0;
    location_.orientation.rollRadians = 0.0;
    wgen::validatePlanetLocation(location_);

    camera.setGlobalViewTarget(
        wgen::planetLocationPositionInPlanetRadii(
            location_,
            planetRadiusMeters),
        {},
        cameraUp);
    // Rendering is rebased independently of the persistent PlanetLocation.
    // Using the camera itself as the current origin maximizes nearby float
    // precision; patch identities and terrain requests remain global.
    camera.rebaseRenderOrigin(camera.globalPosition());
}

} // namespace lve
