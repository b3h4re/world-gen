#include "game/camera/camera_3d.hpp"
#include "game/input/camera_controller_planet.hpp"
#include "game/input/planet_navigation.hpp"

#include "helpers.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {

constexpr double RADIUS = 6'371'000.0;

void requireVectorNear(
        glm::dvec3 actual,
        glm::dvec3 expected,
        double tolerance,
        const std::string& message) {
    wgen::tests::require(
        glm::length(actual - expected) <= tolerance,
        message);
}

void requirePoseNear(
        const lve::PlanetNavigationCameraPose& actual,
        const lve::PlanetNavigationCameraPose& expected,
        double tolerance,
        const std::string& message) {
    wgen::tests::require(
        glm::length(
            actual.positionInPlanetRadii - expected.positionInPlanetRadii) <=
                tolerance &&
            glm::length(
                actual.targetInPlanetRadii - expected.targetInPlanetRadii) <=
                tolerance &&
            glm::length(actual.up - expected.up) <= tolerance,
        message);
}

void testLocalFrameMappingAndParallelTransport() {
    const std::array directions{
        glm::dvec3{0.0, 0.0, 1.0},
        glm::dvec3{0.0, 0.0, -1.0},
        wgen::cubeSphereDirection(wgen::CubeSphereFace::Front, 1.0, 0.25),
        glm::normalize(glm::dvec3{1.0, 2.0, 3.0}),
    };
    const std::array offsets{
        glm::dvec2{},
        glm::dvec2{125.0, -250.0},
        glm::dvec2{0.03 * RADIUS, 0.02 * RADIUS},
    };
    for (const glm::dvec3 direction : directions) {
        const wgen::PlanetTangentFrame frame =
            wgen::planetTangentFrame(direction);
        for (const glm::dvec2 offset : offsets) {
            const glm::dvec3 mapped = lve::planetDirectionFromLocalOffset(
                frame,
                offset,
                RADIUS);
            const glm::dvec2 roundTrip = lve::planetLocalOffsetFromDirection(
                frame,
                mapped,
                RADIUS);
            wgen::tests::require(
                glm::length(roundTrip - offset) <= 0.00000001 * RADIUS,
                "local tangent offsets should round-trip at poles and face seams");
        }
    }

    const glm::dvec3 from = wgen::cubeSphereDirection(
        wgen::CubeSphereFace::Front,
        0.95,
        0.2);
    const glm::dvec3 to = wgen::cubeSphereDirection(
        wgen::CubeSphereFace::Right,
        -0.95,
        0.2);
    const glm::dvec3 tangent = wgen::planetTangentFrame(from).north;
    const glm::dvec3 transported = lve::parallelTransportPlanetTangent(
        tangent,
        from,
        to);
    wgen::tests::require(
        std::abs(glm::length(transported) - 1.0) <= 0.000000000001 &&
            std::abs(glm::dot(transported, to)) <= 0.000000000001,
        "parallel transport should preserve tangent length across cube faces");
}

void testRegimeHysteresisDoesNotChangePose() {
    lve::PlanetNavigationState state = lve::makePlanetNavigationState(
        {0.0, 1.0, 0.0},
        RADIUS,
        RADIUS,
        {.pitchRadians = -std::numbers::pi / 2.0});
    const lve::AppInputState noInput{};
    const auto setAltitudeAndUpdate = [&](double altitudeRadii) {
        state.location.altitudeMeters = altitudeRadii * RADIUS;
        const lve::PlanetNavigationCameraPose before =
            lve::planetNavigationCameraPose(state, RADIUS);
        lve::updatePlanetNavigationState(state, noInput, 0.0, RADIUS);
        const lve::PlanetNavigationCameraPose after =
            lve::planetNavigationCameraPose(state, RADIUS);
        requirePoseNear(
            after,
            before,
            0.000000000001,
            "changing navigation regime should not change camera pose");
    };

    wgen::tests::require(
        state.regime == lve::PlanetNavigationRegime::Orbital,
        "initial high-altitude navigation should be orbital");
    setAltitudeAndUpdate(0.079);
    wgen::tests::require(
        state.regime == lve::PlanetNavigationRegime::Transition,
        "orbital navigation should enter transition below its exit threshold");
    setAltitudeAndUpdate(0.085);
    wgen::tests::require(
        state.regime == lve::PlanetNavigationRegime::Transition,
        "transition should not return to orbital before its enter threshold");
    setAltitudeAndUpdate(0.101);
    wgen::tests::require(
        state.regime == lve::PlanetNavigationRegime::Orbital,
        "transition should enter orbital above its enter threshold");
    setAltitudeAndUpdate(0.079);
    setAltitudeAndUpdate(0.014);
    wgen::tests::require(
        state.regime == lve::PlanetNavigationRegime::Local,
        "transition should enter local below its enter threshold");
    setAltitudeAndUpdate(0.02);
    wgen::tests::require(
        state.regime == lve::PlanetNavigationRegime::Local,
        "local navigation should remain local inside its hysteresis band");
    setAltitudeAndUpdate(0.026);
    setAltitudeAndUpdate(0.02);
    wgen::tests::require(
        state.regime == lve::PlanetNavigationRegime::Transition,
        "transition should not return to local before its enter threshold");

    const double localWeight = lve::planetLocalControlWeight(
        0.02 * RADIUS,
        RADIUS);
    const double transitionWeight = lve::planetLocalControlWeight(
        0.05 * RADIUS,
        RADIUS);
    const double orbitalWeight = lve::planetLocalControlWeight(
        0.09 * RADIUS,
        RADIUS);
    wgen::tests::require(
        localWeight == 1.0 && transitionWeight > 0.0 &&
            transitionWeight < 1.0 && orbitalWeight == 0.0,
        "control interpretation should blend continuously between regimes");
}

void testZoomRoundTripAndPersistentControllerState() {
    lve::PlanetNavigationState state = lve::makePlanetNavigationState(
        {0.0, 1.0, 0.0},
        RADIUS,
        RADIUS,
        {.pitchRadians = -std::numbers::pi / 2.0});
    const lve::PlanetNavigationState initial = state;
    lve::AppInputState zoomIn{};
    zoomIn.cameraZoomIn = true;
    lve::updatePlanetNavigationState(state, zoomIn, 0.5, RADIUS);
    lve::AppInputState zoomOut{};
    zoomOut.cameraZoomOut = true;
    lve::updatePlanetNavigationState(state, zoomOut, 0.5, RADIUS);

    requireVectorNear(
        state.location.unitSurfaceDirection,
        initial.location.unitSurfaceDirection,
        0.0,
        "zooming should not drift across the globe");
    wgen::tests::require(
        std::abs(state.location.altitudeMeters -
            initial.location.altitudeMeters) <= 0.000000001 &&
            state.headingDirection == initial.headingDirection &&
            state.localFrameRevision == initial.localFrameRevision,
        "equal logarithmic zoom steps should restore the complete navigation pose");

    lve::CameraControllerPlanet controller;
    lve::Camera3d camera;
    controller.update({}, 0.0F, RADIUS, camera);
    const lve::PlanetNavigationState controllerInitial =
        controller.navigationState();
    lve::AppInputState move{};
    move.cameraMoveRight = true;
    controller.update(move, 0.1F, RADIUS, camera);
    const glm::dvec3 movedDirection = controller.location().unitSurfaceDirection;
    wgen::tests::require(
        movedDirection != controllerInitial.location.unitSurfaceDirection,
        "the controller should update its persistent navigation state");
    controller.update({}, 0.0F, RADIUS, camera);
    wgen::tests::require(
        controller.location().unitSurfaceDirection == movedDirection,
        "deriving the camera again should not reset navigation state");
    const lve::PlanetNavigationCameraPose pose =
        lve::planetNavigationCameraPose(controller.navigationState(), RADIUS);
    requireVectorNear(
        camera.globalPosition(),
        pose.positionInPlanetRadii,
        0.000000000001,
        "the render camera should be derived from persistent navigation state");
}

void testLocalMovementCrossesFacesAndReanchors() {
    lve::PlanetNavigationState state = lve::makePlanetNavigationState(
        {0.0, 1.0, 0.0},
        0.01 * RADIUS,
        RADIUS,
        {.pitchRadians = -std::numbers::pi / 2.0});
    lve::PlanetNavigationConfig config;
    config.localMinimumSpeedRadiiPerSecond = 1.0;
    config.localMaximumSpeedRadiiPerSecond = 1.0;
    config.localReanchorDistanceRadii = 0.05;
    const wgen::CubeSphereFace initialFace = state.location.faceUv->face;
    const std::uint64_t initialRevision = state.localFrameRevision;
    lve::AppInputState moveRight{};
    moveRight.cameraMoveRight = true;
    lve::updatePlanetNavigationState(
        state,
        moveRight,
        1.0,
        RADIUS,
        config);

    lve::validatePlanetNavigationState(state, RADIUS);
    wgen::tests::require(
        state.location.faceUv && state.location.faceUv->face != initialFace,
        "local horizontal movement should cross cube faces without special input logic");
    wgen::tests::require(
        state.localFrameRevision > initialRevision &&
            state.localOffsetMeters == glm::dvec2{},
        "movement beyond the local threshold should re-anchor its frame");
    wgen::tests::require(
        std::abs(glm::dot(
            state.headingDirection,
            state.location.unitSurfaceDirection)) <= 0.000000000001,
        "heading should remain tangent after crossing a face");
}

void testExplicitReanchorPreservesCameraPose() {
    lve::PlanetNavigationState state = lve::makePlanetNavigationState(
        wgen::cubeSphereDirection(wgen::CubeSphereFace::Right, 0.3, -0.2),
        0.01 * RADIUS,
        RADIUS,
        {
            .headingRadians = 0.7,
            .pitchRadians = -std::numbers::pi / 2.0,
        });
    lve::PlanetNavigationConfig config;
    config.localMinimumSpeedRadiiPerSecond = 0.005;
    config.localMaximumSpeedRadiiPerSecond = 0.005;
    config.localReanchorDistanceRadii = 0.1;
    lve::AppInputState move{};
    move.cameraMoveUp = true;
    lve::updatePlanetNavigationState(state, move, 1.0, RADIUS, config);
    wgen::tests::require(
        glm::length(state.localOffsetMeters) > 0.0,
        "small local motion should accumulate before re-anchoring");

    const lve::PlanetNavigationCameraPose before =
        lve::planetNavigationCameraPose(state, RADIUS);
    const wgen::PlanetLocation locationBefore = state.location;
    const glm::dvec3 headingBefore = state.headingDirection;
    const std::uint64_t revisionBefore = state.localFrameRevision;
    lve::reanchorPlanetNavigationState(state, RADIUS);
    const lve::PlanetNavigationCameraPose after =
        lve::planetNavigationCameraPose(state, RADIUS);

    requirePoseNear(
        after,
        before,
        0.000000000001,
        "re-anchoring should preserve the exact camera pose");
    wgen::tests::require(
        state.location == locationBefore &&
            state.headingDirection == headingBefore &&
            state.localOffsetMeters == glm::dvec2{} &&
            state.localFrameRevision == revisionBefore + 1,
        "re-anchoring should only replace local frame placement");
}

void testLocalPathRoundTripAcrossReanchors() {
    lve::PlanetNavigationState state = lve::makePlanetNavigationState(
        {0.0, 1.0, 0.0},
        0.01 * RADIUS,
        RADIUS,
        {.pitchRadians = -std::numbers::pi / 2.0});
    const lve::PlanetNavigationState initial = state;
    lve::PlanetNavigationConfig config;
    config.localMinimumSpeedRadiiPerSecond = 0.2;
    config.localMaximumSpeedRadiiPerSecond = 0.2;
    config.localReanchorDistanceRadii = 0.02;
    lve::AppInputState right{};
    right.cameraMoveRight = true;
    lve::AppInputState left{};
    left.cameraMoveLeft = true;
    for (std::size_t step = 0; step < 500; ++step) {
        lve::updatePlanetNavigationState(
            state,
            right,
            0.016,
            RADIUS,
            config);
    }
    for (std::size_t step = 0; step < 500; ++step) {
        lve::updatePlanetNavigationState(
            state,
            left,
            0.016,
            RADIUS,
            config);
    }

    lve::validatePlanetNavigationState(state, RADIUS);
    requireVectorNear(
        state.location.unitSurfaceDirection,
        initial.location.unitSurfaceDirection,
        0.0000000001,
        "reversing local movement should return to the same globe location");
    requireVectorNear(
        state.headingDirection,
        initial.headingDirection,
        0.0000000001,
        "parallel-transported heading should survive a long reversed path");
    wgen::tests::require(
        state.localFrameRevision > initial.localFrameRevision,
        "the round trip should exercise multiple local-frame reanchors");
}

void testGlobalScreenRaysUseTheNavigationCamera() {
    for (const double altitudeRadii : {0.01, 0.05, 1.0}) {
        const lve::PlanetNavigationState state =
            lve::makePlanetNavigationState(
                glm::normalize(glm::dvec3{1.0, 2.0, 3.0}),
                altitudeRadii * RADIUS,
                RADIUS,
                {
                    .headingRadians = 0.4,
                    .pitchRadians = -std::numbers::pi / 2.0,
                });
        const lve::PlanetNavigationCameraPose pose =
            lve::planetNavigationCameraPose(state, RADIUS);
        lve::Camera3d camera;
        camera.setPerspectiveProjection(0.9F, 16.0F / 9.0F, 0.00001F, 20.0F);
        camera.setGlobalViewTarget(
            pose.positionInPlanetRadii,
            pose.targetInPlanetRadii,
            pose.up);
        camera.rebaseRenderOrigin(camera.globalPosition());

        const lve::CameraRay3d center = camera.globalScreenRay(0.0, 0.0);
        requireVectorNear(
            center.origin,
            camera.globalPosition(),
            0.0,
            "screen rays should start at the persistent global camera position");
        requireVectorNear(
            center.direction,
            camera.globalForward(),
            0.000000000001,
            "center picking rays should use the camera derived in every regime");
        const lve::CameraRay3d topRight = camera.globalScreenRay(1.0, -1.0);
        wgen::tests::require(
            glm::dot(topRight.direction, camera.globalRight()) > 0.0 &&
                glm::dot(topRight.direction, camera.globalUp()) > 0.0,
            "screen-ray axes should agree with the rendered camera basis");

        camera.rebaseRenderOrigin(camera.globalPosition() + glm::dvec3{1.0});
        const lve::CameraRay3d rebased = camera.globalScreenRay(0.0, 0.0);
        wgen::tests::require(
            rebased.origin == center.origin &&
                rebased.direction == center.direction,
            "render-origin rebasing should not affect global picking rays");
    }

    lve::Camera3d camera;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&camera] { camera.globalScreenRay(0.0, 0.0); },
        "screen rays should require an initialized projection");
    camera.setPerspectiveProjection(0.9F, 1.0F, 0.1F, 10.0F);
    wgen::tests::requireThrows<std::invalid_argument>(
        [&camera] {
            camera.globalScreenRay(
                std::numeric_limits<double>::quiet_NaN(),
                0.0);
        },
        "screen rays should reject non-finite coordinates");
}

void testNavigationValidation() {
    lve::PlanetNavigationConfig invalid;
    invalid.localExitAltitudeRadii = invalid.localEnterAltitudeRadii;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&invalid] { lve::validatePlanetNavigationConfig(invalid); },
        "navigation should reject overlapping hysteresis thresholds");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            lve::makePlanetNavigationState(
                {0.0, 1.0, 0.0},
                -1.0,
                RADIUS);
        },
        "navigation should reject a negative altitude");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            lve::planetDirectionFromLocalOffset(
                wgen::planetTangentFrame({0.0, 1.0, 0.0}),
                {std::numbers::pi * RADIUS, 0.0},
                RADIUS);
        },
        "local mapping should reject an ambiguous antipodal offset");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest(
            "local frame mapping and parallel transport",
            testLocalFrameMappingAndParallelTransport);
        wgen::tests::runTest(
            "regime hysteresis does not change pose",
            testRegimeHysteresisDoesNotChangePose);
        wgen::tests::runTest(
            "zoom round trip and persistent controller state",
            testZoomRoundTripAndPersistentControllerState);
        wgen::tests::runTest(
            "local movement crosses faces and reanchors",
            testLocalMovementCrossesFacesAndReanchors);
        wgen::tests::runTest(
            "explicit reanchor preserves camera pose",
            testExplicitReanchorPreservesCameraPose);
        wgen::tests::runTest(
            "local path round trip across reanchors",
            testLocalPathRoundTripAcrossReanchors);
        wgen::tests::runTest(
            "global screen rays use navigation camera",
            testGlobalScreenRaysUseTheNavigationCamera);
        wgen::tests::runTest(
            "navigation validation",
            testNavigationValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
