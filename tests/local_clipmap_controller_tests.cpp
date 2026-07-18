#include "terrain/planet/local_clipmap_controller.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>

namespace {

wgen::PlanetPatchId patchAt(
        glm::dvec3 direction,
        std::uint8_t level,
        std::optional<wgen::CubeSphereFace> preferred = std::nullopt) {
    const wgen::CubeSphereFaceUv address =
        wgen::directionToCubeSphereFaceUv(direction, preferred);
    const std::uint32_t count = wgen::patchesPerAxis(level);
    const auto coordinate = [count](double value) {
        const double scaled = (value + 1.0) * 0.5 * count;
        return std::min(
            static_cast<std::uint32_t>(std::max(0.0, std::floor(scaled))),
            count - 1);
    };
    return {
        address.face,
        level,
        coordinate(address.u),
        coordinate(address.v),
    };
}

wgen::LocalClipmapFootprint footprint(
        glm::dvec3 anchor = {0.0, 0.0, 1.0},
        double radius = 10'000.0) {
    return {
        .frame = wgen::makeLocalPlanetFrame(anchor, radius, 3),
        .centerMeters = {20.0, -10.0},
        .innerHalfExtentMeters = 400.0,
        .outerHalfExtentMeters = 500.0,
        .terrainEpoch = 7,
    };
}

void testControllerHysteresisAndFallback() {
    wgen::LocalClipmapController controller{{
        .activationAltitudeMeters = 10.0,
        .deactivationAltitudeMeters = 20.0,
        .activationGroundResolutionMeters = 1.0,
        .deactivationGroundResolutionMeters = 2.0,
        .coolingDurationSeconds = 0.5,
    }};
    const wgen::LocalClipmapControllerView enter{5.0, 0.5};
    const wgen::LocalClipmapControllerView hysteresis{15.0, 1.5};
    const wgen::LocalClipmapControllerView leave{21.0, 2.1};

    controller.update(leave, false, 0.1);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Inactive,
        "far view should leave the local clipmap inactive");
    controller.update(enter, false, 0.1);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Warming &&
            controller.needsResidency() && !controller.ownsCoverage(),
        "activation should warm without taking global coverage");
    controller.update(enter, false, 2.0);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Warming,
        "delayed clipmap work should preserve global fallback indefinitely");
    controller.update(enter, true, 0.0);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Active &&
            controller.ownsCoverage(),
        "complete current coverage should activate local ownership");
    controller.update(hysteresis, true, 0.1);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Active,
        "hysteresis band should retain active local ownership");

    controller.update(hysteresis, false, 0.0);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Warming &&
            !controller.ownsCoverage(),
        "lost validity or an epoch change should reveal global terrain immediately");
    controller.update(enter, true, 0.0);
    controller.update(leave, true, 0.0);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Cooling &&
            !controller.ownsCoverage() && !controller.needsResidency(),
        "deactivation should enter a non-owning cooling state");
    controller.update(leave, true, 0.25);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Cooling,
        "cooling should persist for its configured duration");
    controller.update(leave, true, 0.25);
    wgen::tests::require(
        controller.state() == wgen::LocalClipmapControllerState::Inactive,
        "cooling should finish in the inactive state");
}

void testRequestedGroundResolution() {
    const double altitude = 100.0;
    const double fieldOfView = 1.0;
    const std::uint32_t viewportHeight = 1'000;
    const double expected =
        2.0 * altitude * std::tan(fieldOfView / 2.0) / viewportHeight;
    const double actual =
        wgen::localClipmapRequestedGroundResolutionMeters(
            altitude,
            fieldOfView,
            viewportHeight);
    wgen::tests::require(
        std::abs(actual - expected) < 1.0e-12,
        "activation should use the physical vertical ground pixel footprint");
}

void testCoverageAndComplementaryDither() {
    const wgen::LocalClipmapFootprint localFootprint = footprint();
    wgen::tests::require(
        wgen::localClipmapCoverage(localFootprint, {20.0, -10.0}) == 1.0 &&
            wgen::localClipmapCoverage(localFootprint, {420.0, -10.0}) == 1.0 &&
            wgen::localClipmapCoverage(localFootprint, {520.0, -10.0}) == 0.0,
        "coverage should be opaque inside and global-only outside");
    const double middle = wgen::localClipmapCoverage(
        localFootprint,
        {470.0, -10.0});
    wgen::tests::require(
        middle > 0.0 && middle < 1.0,
        "overlap band should have fractional analytic coverage");

    for (std::uint32_t y = 0; y < 8; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            for (const double coverage : {0.0, 0.125, 0.5, 0.875, 1.0}) {
                const bool local = wgen::localClipmapDitherKeepsLocal(
                    coverage,
                    x,
                    y);
                const bool global = wgen::localClipmapDitherKeepsGlobal(
                    coverage,
                    x,
                    y);
                wgen::tests::require(
                    local != global,
                    "local and global dither ownership must be exactly complementary");
            }
        }
    }
}

void testConservativePatchSuppressionAndFaceEdges() {
    const wgen::LocalClipmapFootprint localFootprint = footprint();
    const wgen::PlanetPatchId center = patchAt(
        localFootprint.frame.anchorDirection,
        9);
    wgen::tests::require(
        wgen::planetPatchFullyInsideLocalClipmap(center, localFootprint),
        "a sufficiently fine patch at the anchor should be locally owned");
    wgen::tests::require(
        wgen::localClipmapOwnsPlanetPatch(
            center,
            localFootprint,
            true,
            localFootprint.terrainEpoch) &&
            !wgen::localClipmapOwnsPlanetPatch(
                center,
                localFootprint,
                false,
                localFootprint.terrainEpoch) &&
            !wgen::localClipmapOwnsPlanetPatch(
                center,
                localFootprint,
                true,
                localFootprint.terrainEpoch + 1),
        "patch suppression should require current levels and the active terrain epoch");

    const glm::dvec3 boundaryDirection = wgen::localPlanetSurfaceDirection(
        localFootprint.frame,
        {localFootprint.centerMeters.x +
            localFootprint.innerHalfExtentMeters, 0.0});
    const wgen::PlanetPatchId boundary = patchAt(boundaryDirection, 9);
    wgen::tests::require(
        !wgen::planetPatchFullyInsideLocalClipmap(
            boundary,
            localFootprint),
        "a patch intersecting the inner boundary must remain global fallback");

    const glm::dvec3 edgeAnchor = glm::normalize(glm::dvec3{1.0, 0.0, 1.0});
    wgen::LocalClipmapFootprint edgeFootprint = footprint(edgeAnchor);
    edgeFootprint.centerMeters = {};
    const wgen::PlanetPatchId top = patchAt(
        edgeAnchor,
        12,
        wgen::CubeSphereFace::Top);
    const wgen::PlanetPatchId right = patchAt(
        edgeAnchor,
        12,
        wgen::CubeSphereFace::Right);
    wgen::tests::require(
        wgen::planetPatchFullyInsideLocalClipmap(top, edgeFootprint) &&
            wgen::planetPatchFullyInsideLocalClipmap(right, edgeFootprint),
        "coverage ownership should cross a cube-face edge without a face special case");
}

void testTangentMappingRoundTrip() {
    const wgen::LocalClipmapFootprint localFootprint = footprint(
        glm::dvec3{0.1, 0.97, -0.2});
    for (const glm::dvec2 local : {
            glm::dvec2{},
            glm::dvec2{20.0, -30.0},
            glm::dvec2{-450.0, 100.0}}) {
        const glm::dvec3 direction = wgen::localPlanetSurfaceDirection(
            localFootprint.frame,
            local);
        const glm::dvec2 recovered = wgen::localPlanetTangentPosition(
            localFootprint.frame,
            direction);
        wgen::tests::require(
            glm::length(local - recovered) < 1.0e-9,
            "hybrid tangent mapping should invert the clipmap surface mapping");
    }
}

} // namespace

int main() {
    try {
        wgen::tests::runTest(
            "controller hysteresis and automatic fallback",
            testControllerHysteresisAndFallback);
        wgen::tests::runTest(
            "requested ground resolution",
            testRequestedGroundResolution);
        wgen::tests::runTest(
            "coverage and complementary dither",
            testCoverageAndComplementaryDither);
        wgen::tests::runTest(
            "conservative suppression across face edges",
            testConservativePatchSuppressionAndFaceEdges);
        wgen::tests::runTest(
            "tangent mapping round trip",
            testTangentMappingRoundTrip);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
