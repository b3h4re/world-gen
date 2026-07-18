#include "camera_controller_planet.hpp"

#include <cmath>
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
    if (!locationInitialized_) {
        navigationState_ = makePlanetNavigationState(
            {0.0, 1.0, 0.0},
            planetRadiusMeters,
            planetRadiusMeters,
            {
                .headingRadians = -std::numbers::pi / 2.0,
                .pitchRadians = -std::numbers::pi / 2.0,
            });
        locationInitialized_ = true;
    }

    updatePlanetNavigationState(
        navigationState_,
        input,
        frameTime,
        planetRadiusMeters,
        {},
        localControlWeightOverride_);
    const PlanetNavigationCameraPose pose = planetNavigationCameraPose(
        navigationState_,
        planetRadiusMeters);
    camera.setGlobalViewTarget(
        pose.positionInPlanetRadii,
        pose.targetInPlanetRadii,
        pose.up);
    // Rendering is rebased independently of the persistent PlanetLocation.
    // Using the camera itself as the current origin maximizes nearby float
    // precision; patch identities and terrain requests remain global.
    camera.rebaseRenderOrigin(camera.globalPosition());
}

void CameraControllerPlanet::setLocalControlWeightOverride(
        std::optional<double> value) {
    if (value && (!std::isfinite(*value) || *value < 0.0 || *value > 1.0)) {
        throw std::invalid_argument{
            "planet local control override must be between zero and one"};
    }
    localControlWeightOverride_ = value;
}

} // namespace lve
