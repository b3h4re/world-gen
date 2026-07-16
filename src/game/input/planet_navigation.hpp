#pragma once

#include "game/input/input_state.hpp"
#include "terrain/planet/planet_location.hpp"

#include <cstdint>

#include <glm/glm.hpp>

namespace lve {

enum class PlanetNavigationRegime : std::uint8_t {
    Local,
    Transition,
    Orbital,
};

struct PlanetNavigationConfig {
    double minimumAltitudeRadii{0.000001};
    double maximumAltitudeRadii{7.0};
    double localEnterAltitudeRadii{0.015};
    double localExitAltitudeRadii{0.025};
    double orbitalExitAltitudeRadii{0.08};
    double orbitalEnterAltitudeRadii{0.1};
    double localReanchorDistanceRadii{0.01};
    double orbitalAngularSpeedRadiansPerSecond{1.5};
    double localMinimumSpeedRadiiPerSecond{0.00001};
    double localMaximumSpeedRadiiPerSecond{0.02};
    double localAltitudeSpeedScale{0.75};
    double zoomLogRatePerSecond{2.0};
};

struct PlanetNavigationState {
    wgen::PlanetLocation location{};
    // The local frame is anchored independently from the current surface
    // direction. localOffsetMeters maps from this anchor to location.
    wgen::PlanetTangentFrame localFrame{};
    glm::dvec2 localOffsetMeters{};
    // Unit tangent vector pointing toward the top of the rendered view.
    glm::dvec3 headingDirection{};
    PlanetNavigationRegime regime{PlanetNavigationRegime::Orbital};
    std::uint64_t localFrameRevision{1};
};

struct PlanetNavigationCameraPose {
    glm::dvec3 positionInPlanetRadii{};
    glm::dvec3 targetInPlanetRadii{};
    glm::dvec3 up{};
};

void validatePlanetNavigationConfig(const PlanetNavigationConfig& config);
void validatePlanetNavigationState(
    const PlanetNavigationState& state,
    double planetRadiusMeters);

PlanetNavigationState makePlanetNavigationState(
    glm::dvec3 surfaceDirection,
    double altitudeMeters,
    double planetRadiusMeters,
    wgen::PlanetOrientation orientation = {});

double planetLocalControlWeight(
    double altitudeMeters,
    double planetRadiusMeters,
    const PlanetNavigationConfig& config = {});

glm::dvec3 planetDirectionFromLocalOffset(
    const wgen::PlanetTangentFrame& frame,
    glm::dvec2 offsetMeters,
    double planetRadiusMeters);

glm::dvec2 planetLocalOffsetFromDirection(
    const wgen::PlanetTangentFrame& frame,
    glm::dvec3 surfaceDirection,
    double planetRadiusMeters);

glm::dvec3 parallelTransportPlanetTangent(
    glm::dvec3 tangent,
    glm::dvec3 fromDirection,
    glm::dvec3 toDirection);

void reanchorPlanetNavigationState(
    PlanetNavigationState& state,
    double planetRadiusMeters);

void updatePlanetNavigationState(
    PlanetNavigationState& state,
    const AppInputState& input,
    double frameTimeSeconds,
    double planetRadiusMeters,
    const PlanetNavigationConfig& config = {});

PlanetNavigationCameraPose planetNavigationCameraPose(
    const PlanetNavigationState& state,
    double planetRadiusMeters);

} // namespace lve
