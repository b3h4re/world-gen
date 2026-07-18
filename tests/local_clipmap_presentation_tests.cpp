#include "terrain/planet/local_clipmap_presentation.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

wgen::LocalClipmapFootprint footprint() {
    return {
        .frame = wgen::makeLocalPlanetFrame(
            glm::normalize(glm::dvec3{1.0, 2.0, 3.0}),
            10'000.0,
            4),
        .centerMeters = {20.0, -30.0},
        .innerHalfExtentMeters = 400.0,
        .outerHalfExtentMeters = 500.0,
        .terrainEpoch = 8,
    };
}

void testAltitudeAndRegimeBlend() {
    constexpr double radius = 10'000.0;
    const wgen::LocalClipmapPresentationConfig config{};
    const auto amount = [&](double altitudeRadii, bool local) {
        return wgen::localClipmapFlattenAmount({
            .altitudeMeters = altitudeRadii * radius,
            .planetRadiusMeters = radius,
            .localNavigationRegime = local,
        }, config);
    };
    wgen::tests::require(
        amount(config.fullyFlatAltitudeRadii, true) == 1.0 &&
            amount(config.fullyCurvedAltitudeRadii, true) == 0.0 &&
            amount(0.009, true) > 0.0 &&
            amount(0.009, true) < 1.0,
        "geometry flattening should blend smoothly across its altitude interval");
    wgen::tests::require(
        amount(0.001, false) == 0.0,
        "non-local navigation should keep presentation spherical");
}

void testCenterMaskProtectsHandoff() {
    const wgen::LocalClipmapFootprint localFootprint = footprint();
    const wgen::LocalClipmapPresentationState presentation =
        wgen::makeLocalClipmapPresentationState(
            localFootprint,
            {
                .altitudeMeters = 1.0,
                .planetRadiusMeters = 10'000.0,
                .localNavigationRegime = true,
            });
    wgen::tests::require(
        wgen::localClipmapPresentationCenterMask(
            localFootprint,
            presentation,
            presentation.centerMeters) == 1.0,
        "the clipmap center should receive the full geometry blend");
    const glm::dvec2 curvedPosition = presentation.centerMeters +
        glm::dvec2{presentation.curvedBoundaryHalfExtentMeters, 0.0};
    wgen::tests::require(
        wgen::localClipmapPresentationCenterMask(
            localFootprint,
            presentation,
            curvedPosition) == 0.0 &&
            std::max(
                std::abs(presentation.centerMeters.x -
                    localFootprint.centerMeters.x),
                std::abs(presentation.centerMeters.y -
                    localFootprint.centerMeters.y)) +
                presentation.curvedBoundaryHalfExtentMeters <
                localFootprint.innerHalfExtentMeters,
        "flattening should end before global/local ownership overlap begins");
    wgen::tests::require(
        wgen::localClipmapPresentationCenterMask(
            localFootprint,
            presentation,
            {localFootprint.centerMeters.x +
                localFootprint.innerHalfExtentMeters, 0.0}) == 0.0,
        "the complete handoff band should stay fully curved");
}

void testPresentationMixAndDebugOverride() {
    const wgen::LocalClipmapFootprint localFootprint = footprint();
    const wgen::LocalClipmapPresentationState presentation =
        wgen::makeLocalClipmapPresentationState(
            localFootprint,
            {
                .altitudeMeters = 200.0,
                .planetRadiusMeters = 10'000.0,
                .localNavigationRegime = false,
            },
            0.25);
    wgen::tests::require(
        presentation.flattenAmount == 0.25,
        "debug scrubbing should freeze the authored geometry blend");

    const glm::dvec3 curved{1.0, 2.0, 3.0};
    const glm::dvec3 tangent{5.0, 6.0, 7.0};
    const glm::dvec3 presented =
        wgen::localClipmapPresentedRelativePosition(
            curved,
            tangent,
            0.5,
            0.5);
    wgen::tests::require(
        glm::length(presented - glm::mix(curved, tangent, 0.25)) < 1.0e-12,
        "presentation should multiply global and spatial blend weights");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest(
            "altitude and regime geometry blend",
            testAltitudeAndRegimeBlend);
        wgen::tests::runTest(
            "center mask protects curved handoff",
            testCenterMaskProtectsHandoff);
        wgen::tests::runTest(
            "presentation mix and debug override",
            testPresentationMixAndDebugOverride);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
