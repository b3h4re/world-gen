#include "terrain/terrain_detail.hpp"

#include "helpers.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {

void expectDoubleNear(
        double actual,
        double expected,
        double epsilon,
        std::string_view message) {
    wgen::tests::require(std::abs(actual - expected) <= epsilon, message);
}

void testReferenceSpacing() {
    constexpr double radius = 6400.0;
    constexpr std::uint32_t quads = 32;
    const wgen::TerrainDetailPolicy policy{radius, quads};
    const double expected = radius * (std::numbers::pi / 2.0) /
        static_cast<double>(quads);

    expectDoubleNear(
        policy.referenceSampleSpacing(),
        expected,
        0.0000001,
        "terrain detail reference spacing is wrong");
    expectDoubleNear(
        policy.sampleSpacingForDetail(wgen::TerrainDetailLevel{0.0}),
        expected,
        0.0000001,
        "detail zero should use the reference spacing");
    expectDoubleNear(
        policy.sampleSpacingForDetail(wgen::TerrainDetailLevel{3.0}),
        expected / 8.0,
        0.0000001,
        "integer detail spacing should halve at each level");
}

void testPatchAndSpacingEquivalence() {
    const wgen::TerrainDetailPolicy policy{6371000.0, 32};
    for (std::uint8_t level = 0; level <= 12; ++level) {
        const double patchSpacing = policy.cubeFacePatchSampleSpacing(level, 32);
        const wgen::TerrainDetailLevel patchDetail =
            policy.detailForCubeFacePatch(level, 32);
        const wgen::TerrainDetailLevel clipmapDetail =
            policy.equivalentDetailForSpacing(patchSpacing);

        expectDoubleNear(
            patchDetail.value(),
            static_cast<double>(level),
            0.0000001,
            "quadtree patch level should map through physical spacing");
        expectDoubleNear(
            clipmapDetail.value(),
            patchDetail.value(),
            0.0000001,
            "equivalent clipmap spacing should request the same terrain detail");
    }

    expectDoubleNear(
        policy.detailForCubeFacePatch(3, 64).value(),
        4.0,
        0.0000001,
        "doubling patch resolution should add one terrain detail level");
}

void testContinuousDetailAndClamping() {
    const wgen::TerrainDetailPolicy policy{100.0, 32};
    const double levelTwoSpacing =
        policy.sampleSpacingForDetail(wgen::TerrainDetailLevel{2.0});
    const double levelThreeSpacing =
        policy.sampleSpacingForDetail(wgen::TerrainDetailLevel{3.0});
    const double halfwaySpacing = std::sqrt(levelTwoSpacing * levelThreeSpacing);

    expectDoubleNear(
        policy.equivalentDetailForSpacing(halfwaySpacing).value(),
        2.5,
        0.0000001,
        "geometric mean spacing should map to a fractional detail level");
    expectDoubleNear(
        policy.equivalentDetailForSpacing(policy.referenceSampleSpacing() * 4.0).value(),
        0.0,
        0.0,
        "coarser-than-reference spacing should clamp to detail zero");
    expectDoubleNear(
        policy.equivalentDetailForSpacing(
            policy.sampleSpacingForDetail(
                wgen::TerrainDetailLevel{static_cast<double>(wgen::MAX_TERRAIN_DETAIL_LEVEL)}) /
            4.0).value(),
        static_cast<double>(wgen::MAX_TERRAIN_DETAIL_LEVEL),
        0.0,
        "finer-than-supported spacing should clamp to maximum detail");
}

void testBandFade() {
    const wgen::TerrainDetailBand levelZero =
        wgen::terrainDetailBandForFirstFullyVisibleLevel(0);
    wgen::tests::expectNear(
        wgen::terrainDetailBandWeight(wgen::TerrainDetailLevel{0.0}, levelZero),
        1.0F,
        0.0F,
        "level-zero terrain should always be fully visible");

    const wgen::TerrainDetailBand levelTwo =
        wgen::terrainDetailBandForFirstFullyVisibleLevel(2);
    wgen::tests::expectNear(
        wgen::terrainDetailBandWeight(wgen::TerrainDetailLevel{1.0}, levelTwo),
        0.0F,
        0.0F,
        "a detail band should be absent at its fade start");
    wgen::tests::expectNear(
        wgen::terrainDetailBandWeight(wgen::TerrainDetailLevel{1.5}, levelTwo),
        0.5F,
        0.000001F,
        "a detail band should fade smoothly between integer levels");
    wgen::tests::expectNear(
        wgen::terrainDetailBandWeight(wgen::TerrainDetailLevel{2.0}, levelTwo),
        1.0F,
        0.0F,
        "a detail band should be complete at its fully visible level");
}

void testValidation() {
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::TerrainDetailPolicy{0.0, 32}; },
        "terrain detail policy should reject zero radius");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::TerrainDetailPolicy{100.0, 0}; },
        "terrain detail policy should reject zero reference quads");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::TerrainDetailLevel{-0.01}; },
        "terrain detail should reject negative levels");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::TerrainDetailLevel{std::numeric_limits<double>::infinity()}; },
        "terrain detail should reject non-finite levels");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::TerrainDetailLevel{
                static_cast<double>(wgen::MAX_TERRAIN_DETAIL_LEVEL) + 0.01};
        },
        "terrain detail should reject levels above its maximum");

    const wgen::TerrainDetailPolicy policy{100.0, 32};
    wgen::tests::requireThrows<std::invalid_argument>(
        [&policy] { policy.equivalentDetailForSpacing(0.0); },
        "terrain detail policy should reject zero sample spacing");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&policy] { policy.cubeFacePatchSampleSpacing(0, 0); },
        "terrain detail policy should reject zero patch quads");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::terrainDetailBandWeight(
                wgen::TerrainDetailLevel{1.0},
                {
                    .fadeStart = wgen::TerrainDetailLevel{2.0},
                    .fullyVisible = wgen::TerrainDetailLevel{1.0},
                });
        },
        "terrain detail weight should reject a reversed band");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("terrain detail reference spacing", testReferenceSpacing);
        wgen::tests::runTest("terrain detail renderer equivalence", testPatchAndSpacingEquivalence);
        wgen::tests::runTest("terrain detail continuous mapping", testContinuousDetailAndClamping);
        wgen::tests::runTest("terrain detail band fade", testBandFade);
        wgen::tests::runTest("terrain detail validation", testValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
