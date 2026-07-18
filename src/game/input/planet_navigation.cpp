#include "planet_navigation.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace lve {
namespace {

constexpr double VECTOR_EPSILON = 1.0e-15;
constexpr double FRAME_EPSILON = 1.0e-10;

bool isFinite(glm::dvec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool isFinite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

void validateRadius(double planetRadiusMeters) {
    if (!std::isfinite(planetRadiusMeters) || planetRadiusMeters <= 0.0) {
        throw std::invalid_argument{
            "planet navigation radius must be finite and positive"};
    }
}

void validateFrame(const wgen::PlanetTangentFrame& frame) {
    if (!isFinite(frame.east) || !isFinite(frame.north) ||
            !isFinite(frame.up) ||
            std::abs(glm::length(frame.east) - 1.0) > FRAME_EPSILON ||
            std::abs(glm::length(frame.north) - 1.0) > FRAME_EPSILON ||
            std::abs(glm::length(frame.up) - 1.0) > FRAME_EPSILON ||
            std::abs(glm::dot(frame.east, frame.north)) > FRAME_EPSILON ||
            std::abs(glm::dot(frame.east, frame.up)) > FRAME_EPSILON ||
            std::abs(glm::dot(frame.north, frame.up)) > FRAME_EPSILON) {
        throw std::invalid_argument{
            "planet navigation local frame must be orthonormal"};
    }
}

glm::dvec3 rotateAroundAxis(
        glm::dvec3 vector,
        glm::dvec3 axis,
        double angle) {
    return vector * std::cos(angle) +
        glm::cross(axis, vector) * std::sin(angle) +
        axis * glm::dot(axis, vector) * (1.0 - std::cos(angle));
}

void updateHeadingAngle(PlanetNavigationState& state) {
    const wgen::PlanetTangentFrame canonical =
        wgen::planetTangentFrame(state.location.unitSurfaceDirection);
    state.headingDirection = glm::normalize(
        state.headingDirection -
        state.location.unitSurfaceDirection * glm::dot(
            state.headingDirection,
            state.location.unitSurfaceDirection));
    state.location.orientation.headingRadians = std::atan2(
        glm::dot(state.headingDirection, canonical.east),
        glm::dot(state.headingDirection, canonical.north));
}

PlanetNavigationRegime initialRegime(
        double altitudeRadii,
        const PlanetNavigationConfig& config) {
    if (altitudeRadii <= config.localExitAltitudeRadii) {
        return PlanetNavigationRegime::Local;
    }
    if (altitudeRadii >= config.orbitalExitAltitudeRadii) {
        return PlanetNavigationRegime::Orbital;
    }
    return PlanetNavigationRegime::Transition;
}

void updateRegime(
        PlanetNavigationState& state,
        double altitudeRadii,
        const PlanetNavigationConfig& config) {
    switch (state.regime) {
        case PlanetNavigationRegime::Local:
            if (altitudeRadii > config.localExitAltitudeRadii) {
                state.regime = PlanetNavigationRegime::Transition;
            }
            break;
        case PlanetNavigationRegime::Transition:
            if (altitudeRadii <= config.localEnterAltitudeRadii) {
                state.regime = PlanetNavigationRegime::Local;
            } else if (altitudeRadii >= config.orbitalEnterAltitudeRadii) {
                state.regime = PlanetNavigationRegime::Orbital;
            }
            break;
        case PlanetNavigationRegime::Orbital:
            if (altitudeRadii < config.orbitalExitAltitudeRadii) {
                state.regime = PlanetNavigationRegime::Transition;
            }
            break;
    }
}

void moveSurfaceDirection(
        PlanetNavigationState& state,
        glm::dvec3 tangentDirection,
        double angularDistance,
        double planetRadiusMeters,
        const PlanetNavigationConfig& config) {
    if (angularDistance <= 0.0) {
        return;
    }
    const glm::dvec3 previousDirection = state.location.unitSurfaceDirection;
    const glm::dvec3 tangent = glm::normalize(
        tangentDirection - previousDirection *
            glm::dot(tangentDirection, previousDirection));
    const glm::dvec3 nextDirection = glm::normalize(
        previousDirection * std::cos(angularDistance) +
        tangent * std::sin(angularDistance));
    state.headingDirection = parallelTransportPlanetTangent(
        state.headingDirection,
        previousDirection,
        nextDirection);
    state.location = wgen::reanchorPlanetLocation(
        state.location,
        nextDirection,
        state.location.faceUv
            ? std::optional{state.location.faceUv->face}
            : std::nullopt);
    updateHeadingAngle(state);
    state.localOffsetMeters = planetLocalOffsetFromDirection(
        state.localFrame,
        nextDirection,
        planetRadiusMeters);
    if (glm::length(state.localOffsetMeters) >
            config.localReanchorDistanceRadii * planetRadiusMeters) {
        reanchorPlanetNavigationState(state, planetRadiusMeters);
    }
}

} // namespace

void validatePlanetNavigationConfig(const PlanetNavigationConfig& config) {
    if (!std::isfinite(config.minimumAltitudeRadii) ||
            !std::isfinite(config.maximumAltitudeRadii) ||
            config.minimumAltitudeRadii <= 0.0 ||
            config.maximumAltitudeRadii <= config.minimumAltitudeRadii ||
            !std::isfinite(config.localEnterAltitudeRadii) ||
            !std::isfinite(config.localExitAltitudeRadii) ||
            !std::isfinite(config.orbitalExitAltitudeRadii) ||
            !std::isfinite(config.orbitalEnterAltitudeRadii) ||
            config.localEnterAltitudeRadii <= config.minimumAltitudeRadii ||
            config.localExitAltitudeRadii <= config.localEnterAltitudeRadii ||
            config.orbitalExitAltitudeRadii <= config.localExitAltitudeRadii ||
            config.orbitalEnterAltitudeRadii <= config.orbitalExitAltitudeRadii ||
            config.orbitalEnterAltitudeRadii >= config.maximumAltitudeRadii ||
            !std::isfinite(config.localControlFullAltitudeRadii) ||
            !std::isfinite(config.localControlZeroAltitudeRadii) ||
            config.localControlFullAltitudeRadii <
                config.minimumAltitudeRadii ||
            config.localControlZeroAltitudeRadii <=
                config.localControlFullAltitudeRadii ||
            config.localControlZeroAltitudeRadii >
                config.maximumAltitudeRadii ||
            !std::isfinite(config.localReanchorDistanceRadii) ||
            config.localReanchorDistanceRadii <= 0.0 ||
            !std::isfinite(config.orbitalAngularSpeedRadiansPerSecond) ||
            config.orbitalAngularSpeedRadiansPerSecond <= 0.0 ||
            !std::isfinite(config.localMinimumSpeedRadiiPerSecond) ||
            config.localMinimumSpeedRadiiPerSecond <= 0.0 ||
            !std::isfinite(config.localMaximumSpeedRadiiPerSecond) ||
            config.localMaximumSpeedRadiiPerSecond <
                config.localMinimumSpeedRadiiPerSecond ||
            !std::isfinite(config.localAltitudeSpeedScale) ||
            config.localAltitudeSpeedScale <= 0.0 ||
            !std::isfinite(config.zoomLogRatePerSecond) ||
            config.zoomLogRatePerSecond <= 0.0) {
        throw std::invalid_argument{"planet navigation config is invalid"};
    }
}

void validatePlanetNavigationState(
        const PlanetNavigationState& state,
        double planetRadiusMeters) {
    validateRadius(planetRadiusMeters);
    wgen::validatePlanetLocation(state.location);
    validateFrame(state.localFrame);
    if (state.location.altitudeMeters < 0.0 ||
            !isFinite(state.localOffsetMeters) ||
            !isFinite(state.headingDirection) ||
            std::abs(glm::length(state.headingDirection) - 1.0) >
                FRAME_EPSILON ||
            std::abs(glm::dot(
                state.headingDirection,
                state.location.unitSurfaceDirection)) > FRAME_EPSILON ||
            state.localFrameRevision == 0) {
        throw std::invalid_argument{"planet navigation state is invalid"};
    }
    const glm::dvec3 offsetDirection = planetDirectionFromLocalOffset(
        state.localFrame,
        state.localOffsetMeters,
        planetRadiusMeters);
    if (glm::length(
            offsetDirection - state.location.unitSurfaceDirection) >
            FRAME_EPSILON) {
        throw std::invalid_argument{
            "planet navigation local offset disagrees with its location"};
    }
}

PlanetNavigationState makePlanetNavigationState(
        glm::dvec3 surfaceDirection,
        double altitudeMeters,
        double planetRadiusMeters,
        wgen::PlanetOrientation orientation) {
    validateRadius(planetRadiusMeters);
    const PlanetNavigationConfig config;
    validatePlanetNavigationConfig(config);
    PlanetNavigationState result;
    result.location = wgen::makePlanetLocation(
        surfaceDirection,
        altitudeMeters,
        orientation);
    result.localFrame = wgen::planetTangentFrame(
        result.location.unitSurfaceDirection);
    result.headingDirection = glm::normalize(
        result.localFrame.north * std::cos(orientation.headingRadians) +
        result.localFrame.east * std::sin(orientation.headingRadians));
    result.regime = initialRegime(
        altitudeMeters / planetRadiusMeters,
        config);
    updateHeadingAngle(result);
    validatePlanetNavigationState(result, planetRadiusMeters);
    return result;
}

double planetLocalControlWeight(
        double altitudeMeters,
        double planetRadiusMeters,
        const PlanetNavigationConfig& config,
        std::optional<double> debugOverride) {
    validateRadius(planetRadiusMeters);
    validatePlanetNavigationConfig(config);
    if (!std::isfinite(altitudeMeters) || altitudeMeters < 0.0) {
        throw std::invalid_argument{
            "planet navigation altitude must be finite and non-negative"};
    }
    if (debugOverride) {
        if (!std::isfinite(*debugOverride) ||
                *debugOverride < 0.0 || *debugOverride > 1.0) {
            throw std::invalid_argument{
                "planet local control debug override is invalid"};
        }
        return *debugOverride;
    }
    const double altitudeRadii = altitudeMeters / planetRadiusMeters;
    const double linearWeight = std::clamp(
        (config.localControlZeroAltitudeRadii - altitudeRadii) /
            (config.localControlZeroAltitudeRadii -
                config.localControlFullAltitudeRadii),
        0.0,
        1.0);
    return linearWeight * linearWeight * (3.0 - 2.0 * linearWeight);
}

glm::dvec3 planetDirectionFromLocalOffset(
        const wgen::PlanetTangentFrame& frame,
        glm::dvec2 offsetMeters,
        double planetRadiusMeters) {
    validateFrame(frame);
    validateRadius(planetRadiusMeters);
    if (!isFinite(offsetMeters)) {
        throw std::invalid_argument{
            "planet navigation local offset must be finite"};
    }
    const double distance = glm::length(offsetMeters);
    if (distance <= VECTOR_EPSILON) {
        return frame.up;
    }
    const double angle = distance / planetRadiusMeters;
    if (angle >= std::numbers::pi) {
        throw std::invalid_argument{
            "planet navigation local offset reaches an ambiguous antipode"};
    }
    const glm::dvec3 tangent = glm::normalize(
        frame.east * offsetMeters.x + frame.north * offsetMeters.y);
    return glm::normalize(
        frame.up * std::cos(angle) + tangent * std::sin(angle));
}

glm::dvec2 planetLocalOffsetFromDirection(
        const wgen::PlanetTangentFrame& frame,
        glm::dvec3 surfaceDirection,
        double planetRadiusMeters) {
    validateFrame(frame);
    validateRadius(planetRadiusMeters);
    if (!isFinite(surfaceDirection) ||
            glm::length(surfaceDirection) <= VECTOR_EPSILON) {
        throw std::invalid_argument{
            "planet navigation surface direction must be finite and non-zero"};
    }
    const glm::dvec3 direction = glm::normalize(surfaceDirection);
    const double cosine = std::clamp(glm::dot(frame.up, direction), -1.0, 1.0);
    const glm::dvec3 tangentComponent = direction - frame.up * cosine;
    const double sine = glm::length(tangentComponent);
    const double angle = std::atan2(sine, cosine);
    if (angle >= std::numbers::pi - VECTOR_EPSILON) {
        throw std::invalid_argument{
            "planet navigation direction is antipodal to its local anchor"};
    }
    if (sine <= VECTOR_EPSILON) {
        return {};
    }
    const glm::dvec3 tangent = tangentComponent / sine;
    const double distance = angle * planetRadiusMeters;
    return {
        glm::dot(tangent, frame.east) * distance,
        glm::dot(tangent, frame.north) * distance,
    };
}

glm::dvec3 parallelTransportPlanetTangent(
        glm::dvec3 tangent,
        glm::dvec3 fromDirection,
        glm::dvec3 toDirection) {
    if (!isFinite(tangent) || !isFinite(fromDirection) ||
            !isFinite(toDirection) ||
            glm::length(tangent) <= VECTOR_EPSILON ||
            glm::length(fromDirection) <= VECTOR_EPSILON ||
            glm::length(toDirection) <= VECTOR_EPSILON) {
        throw std::invalid_argument{
            "planet parallel transport vectors must be finite and non-zero"};
    }
    const glm::dvec3 from = glm::normalize(fromDirection);
    const glm::dvec3 to = glm::normalize(toDirection);
    const double tangentLength = glm::length(tangent);
    glm::dvec3 projected = tangent - from * glm::dot(tangent, from);
    if (glm::length(projected) <= VECTOR_EPSILON) {
        throw std::invalid_argument{
            "planet parallel transport vector must be tangent at its source"};
    }
    projected = glm::normalize(projected) * tangentLength;

    const glm::dvec3 cross = glm::cross(from, to);
    const double sine = glm::length(cross);
    const double cosine = std::clamp(glm::dot(from, to), -1.0, 1.0);
    if (sine <= VECTOR_EPSILON) {
        if (cosine < 0.0) {
            throw std::invalid_argument{
                "planet parallel transport across an antipode is ambiguous"};
        }
        return glm::normalize(
            projected - to * glm::dot(projected, to)) * tangentLength;
    }
    const glm::dvec3 axis = cross / sine;
    const double angle = std::atan2(sine, cosine);
    const glm::dvec3 transported = rotateAroundAxis(projected, axis, angle);
    return glm::normalize(
        transported - to * glm::dot(transported, to)) * tangentLength;
}

void reanchorPlanetNavigationState(
        PlanetNavigationState& state,
        double planetRadiusMeters) {
    validatePlanetNavigationState(state, planetRadiusMeters);
    const glm::dvec3 previousAnchor = state.localFrame.up;
    const glm::dvec3 nextAnchor = state.location.unitSurfaceDirection;
    const glm::dvec3 transportedEast = parallelTransportPlanetTangent(
        state.localFrame.east,
        previousAnchor,
        nextAnchor);
    glm::dvec3 north = parallelTransportPlanetTangent(
        state.localFrame.north,
        previousAnchor,
        nextAnchor);
    north = glm::normalize(
        north - nextAnchor * glm::dot(north, nextAnchor));
    glm::dvec3 east = glm::normalize(glm::cross(north, nextAnchor));
    if (glm::dot(east, transportedEast) < 0.0) {
        east = -east;
        north = -north;
    }
    state.localFrame = {
        .east = east,
        .north = north,
        .up = nextAnchor,
    };
    state.localOffsetMeters = {};
    ++state.localFrameRevision;
    if (state.localFrameRevision == 0) {
        state.localFrameRevision = 1;
    }
    validatePlanetNavigationState(state, planetRadiusMeters);
}

void updatePlanetNavigationState(
        PlanetNavigationState& state,
        const AppInputState& input,
        double frameTimeSeconds,
        double planetRadiusMeters,
        const PlanetNavigationConfig& config,
        std::optional<double> localControlWeightOverride) {
    validatePlanetNavigationConfig(config);
    validatePlanetNavigationState(state, planetRadiusMeters);
    if (!std::isfinite(frameTimeSeconds) || frameTimeSeconds < 0.0) {
        throw std::invalid_argument{
            "planet navigation frame time must be finite and non-negative"};
    }

    const double zoomDirection =
        static_cast<double>(input.cameraZoomOut) -
        static_cast<double>(input.cameraZoomIn);
    if (zoomDirection != 0.0 && frameTimeSeconds > 0.0) {
        state.location.altitudeMeters *= std::exp(
            zoomDirection * config.zoomLogRatePerSecond * frameTimeSeconds);
    }
    state.location.altitudeMeters = std::clamp(
        state.location.altitudeMeters,
        config.minimumAltitudeRadii * planetRadiusMeters,
        config.maximumAltitudeRadii * planetRadiusMeters);

    const double altitudeRadii =
        state.location.altitudeMeters / planetRadiusMeters;
    updateRegime(state, altitudeRadii, config);
    const double localWeight = planetLocalControlWeight(
        state.location.altitudeMeters,
        planetRadiusMeters,
        config,
        localControlWeightOverride);

    glm::dvec2 motionInput{
        static_cast<double>(input.cameraMoveRight) -
            static_cast<double>(input.cameraMoveLeft),
        static_cast<double>(input.cameraMoveUp) -
            static_cast<double>(input.cameraMoveDown),
    };
    const double inputLength = glm::length(motionInput);
    if (inputLength > 0.0 && frameTimeSeconds > 0.0) {
        if (inputLength > 1.0) {
            motionInput /= inputLength;
        }
        const glm::dvec3 surfaceDirection =
            state.location.unitSurfaceDirection;
        const glm::dvec3 screenRight = glm::normalize(glm::cross(
            -surfaceDirection,
            state.headingDirection));
        const glm::dvec3 tangentDirection = glm::normalize(
            screenRight * motionInput.x +
            state.headingDirection * motionInput.y);
        const double localLinearSpeedRadii = std::clamp(
            altitudeRadii * config.localAltitudeSpeedScale,
            config.localMinimumSpeedRadiiPerSecond,
            config.localMaximumSpeedRadiiPerSecond);
        const double angularSpeed = std::lerp(
            config.orbitalAngularSpeedRadiansPerSecond,
            localLinearSpeedRadii,
            localWeight);
        moveSurfaceDirection(
            state,
            tangentDirection,
            angularSpeed * frameTimeSeconds,
            planetRadiusMeters,
            config);
    }

    validatePlanetNavigationState(state, planetRadiusMeters);
}

PlanetNavigationCameraPose planetNavigationCameraPose(
        const PlanetNavigationState& state,
        double planetRadiusMeters) {
    validatePlanetNavigationState(state, planetRadiusMeters);
    const glm::dvec3 radialUp = state.location.unitSurfaceDirection;
    const double pitch = state.location.orientation.pitchRadians;
    const double roll = state.location.orientation.rollRadians;
    const glm::dvec3 forward = glm::normalize(
        state.headingDirection * std::cos(pitch) +
        radialUp * std::sin(pitch));
    glm::dvec3 cameraUp = glm::normalize(
        state.headingDirection * -std::sin(pitch) +
        radialUp * std::cos(pitch));
    if (roll != 0.0) {
        cameraUp = glm::normalize(rotateAroundAxis(cameraUp, forward, roll));
    }
    const glm::dvec3 position = wgen::planetLocationPositionInPlanetRadii(
        state.location,
        planetRadiusMeters);
    return {
        .positionInPlanetRadii = position,
        .targetInPlanetRadii = position + forward,
        .up = cameraUp,
    };
}

} // namespace lve
