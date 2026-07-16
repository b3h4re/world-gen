#include "terrain/planet/planet_lod_selector.hpp"

#include "game/camera/camera_3d.hpp"
#include "game/input/planet_camera_depth.hpp"
#include "helpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {

wgen::PlanetLodView makeView(double distance = 3.0) {
    return {
        .position = {0.0, 0.0, distance},
        .forward = {0.0, 0.0, -1.0},
        .right = {1.0, 0.0, 0.0},
        .up = {0.0, 1.0, 0.0},
        .verticalFov = 50.0 * std::numbers::pi / 180.0,
        .aspectRatio = 16.0 / 9.0,
        .nearPlane = 0.1,
        .farPlane = 20.0,
        .viewportHeight = 720,
    };
}

bool isDescendantOrSelf(
        const wgen::PlanetPatchId& id,
        const wgen::PlanetPatchId& ancestor) {
    if (id.face != ancestor.face || id.level < ancestor.level) {
        return false;
    }
    const std::uint8_t difference = static_cast<std::uint8_t>(id.level - ancestor.level);
    return (id.x >> difference) == ancestor.x && (id.y >> difference) == ancestor.y;
}

std::optional<wgen::PlanetPatchId> coveringLeaf(
        wgen::PlanetPatchId id,
        const std::unordered_set<
            wgen::PlanetPatchId,
            wgen::PlanetPatchIdHash>& leaves) {
    while (true) {
        if (leaves.contains(id)) {
            return id;
        }
        const std::optional<wgen::PlanetPatchId> next = wgen::parent(id);
        if (!next) {
            return std::nullopt;
        }
        id = *next;
    }
}

void requireCompletePartition(
        const std::vector<wgen::PlanetPatchId>& leaves,
        std::uint8_t maximumLevel) {
    for (std::size_t i = 0; i < leaves.size(); ++i) {
        wgen::validate(leaves[i]);
        wgen::tests::require(
            leaves[i].level <= maximumLevel,
            "selected leaf exceeds the configured maximum");
        for (std::size_t j = i + 1; j < leaves.size(); ++j) {
            wgen::tests::require(
                !isDescendantOrSelf(leaves[i], leaves[j]) &&
                    !isDescendantOrSelf(leaves[j], leaves[i]),
                "selected leaves overlap");
        }
    }

    const std::uint64_t fullFaceUnits = std::uint64_t{1} << (2 * maximumLevel);
    std::array<std::uint64_t, wgen::FACES.size()> faceUnits{};
    for (const wgen::PlanetPatchId& leaf : leaves) {
        const std::uint8_t levelDifference =
            static_cast<std::uint8_t>(maximumLevel - leaf.level);
        faceUnits[wgen::faceID(leaf.face)] += std::uint64_t{1} << (2 * levelDifference);
    }
    for (const std::uint64_t units : faceUnits) {
        wgen::tests::require(units == fullFaceUnits, "selected leaves do not cover a complete face");
    }
}

void requireBalanced(const std::vector<wgen::PlanetPatchId>& leaves) {
    const std::unordered_set<wgen::PlanetPatchId, wgen::PlanetPatchIdHash> leafSet{
        leaves.begin(),
        leaves.end(),
    };
    constexpr std::array EDGES{
        wgen::PlanetPatchEdge::UMin,
        wgen::PlanetPatchEdge::UMax,
        wgen::PlanetPatchEdge::VMin,
        wgen::PlanetPatchEdge::VMax,
    };
    for (const wgen::PlanetPatchId& leaf : leaves) {
        for (const wgen::PlanetPatchEdge edge : EDGES) {
            const wgen::PlanetPatchId sameLevel = wgen::sameLevelNeighbor(leaf, edge).id;
            const std::optional<wgen::PlanetPatchId> neighbor = coveringLeaf(sameLevel, leafSet);
            if (neighbor) {
                wgen::tests::require(
                    neighbor->level + 1 >= leaf.level,
                    "selected edge neighbors differ by more than one level");
            }
        }
    }
}

void testSelectionCoverageBalanceAndBudget() {
    wgen::PlanetLodConfig config;
    config.maximumLevel = 8;
    config.selectedLeafBudget = 256;
    const wgen::PlanetLodSelection selection = wgen::PlanetLodSelector{}.select(
        makeView(1.4),
        {.radius = 1.0, .minimumDisplacement = -0.01, .maximumDisplacement = 0.01},
        config);

    wgen::tests::require(
        selection.leaves.size() <= config.selectedLeafBudget,
        "selection exceeded its leaf budget");
    requireCompletePartition(selection.leaves, config.maximumLevel);
    requireBalanced(selection.leaves);
}

void testSelectionHysteresis() {
    const wgen::PlanetLodView view = makeView();
    const wgen::PlanetLodSurface surface{
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    };
    const wgen::PlanetPatchId topRoot{wgen::CubeSphereFace::Top, 0, 0, 0};
    const double rootError = wgen::planetPatchScreenErrorPixels(topRoot, view, surface);

    wgen::PlanetLodConfig splitConfig;
    splitConfig.maximumLevel = 1;
    splitConfig.targetErrorPixels = rootError / 1.1;
    const wgen::PlanetLodErrorThresholds thresholds =
        wgen::planetLodErrorThresholds(splitConfig);
    wgen::tests::require(
        thresholds.mergePixels < thresholds.splitPixels,
        "split and merge error thresholds should remain explicitly separated");
    const wgen::PlanetLodSelection split =
        wgen::PlanetLodSelector{}.select(view, surface, splitConfig);
    wgen::tests::require(
        std::find(split.leaves.begin(), split.leaves.end(), topRoot) == split.leaves.end(),
        "root should split above the upper threshold");

    wgen::PlanetLodConfig deadBandConfig = splitConfig;
    deadBandConfig.targetErrorPixels = rootError / 0.85;
    const wgen::PlanetLodSelection retained = wgen::PlanetLodSelector{}.select(
        view,
        surface,
        deadBandConfig,
        split.leaves);
    const wgen::PlanetLodSelection unsplit = wgen::PlanetLodSelector{}.select(
        view,
        surface,
        deadBandConfig);
    wgen::tests::require(
        std::find(retained.leaves.begin(), retained.leaves.end(), topRoot) == retained.leaves.end(),
        "previously split root should remain split inside the hysteresis band");
    wgen::tests::require(
        std::find(unsplit.leaves.begin(), unsplit.leaves.end(), topRoot) != unsplit.leaves.end(),
        "previously unsplit root should remain merged inside the hysteresis band");

    wgen::PlanetLodConfig mergeConfig = splitConfig;
    mergeConfig.targetErrorPixels = rootError / 0.6;
    const wgen::PlanetLodSelection merged = wgen::PlanetLodSelector{}.select(
        view,
        surface,
        mergeConfig,
        retained.leaves);
    wgen::tests::require(
        std::find(merged.leaves.begin(), merged.leaves.end(), topRoot) != merged.leaves.end(),
        "root should merge below the lower threshold");
}

void testCullingAndDeterminism() {
    wgen::PlanetLodConfig config;
    config.maximumLevel = 5;
    config.selectedLeafBudget = 128;
    const wgen::PlanetLodView view = makeView(2.0);
    const wgen::PlanetLodSurface surface{
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    };
    const wgen::PlanetLodSelector selector;
    const wgen::PlanetLodSelection first = selector.select(view, surface, config);
    const wgen::PlanetLodSelection second = selector.select(view, surface, config);

    wgen::tests::require(first.leaves == second.leaves, "selection should be deterministic");
    wgen::tests::require(
        first.visibleLeaves == second.visibleLeaves,
        "visible selection should be deterministic");
    wgen::tests::require(
        first.visibleLeaves.size() < first.leaves.size(),
        "frustum and horizon culling should reject some logical leaves");
    for (const wgen::PlanetPatchId& visible : first.visibleLeaves) {
        wgen::tests::require(
            wgen::isPlanetPatchVisible(visible, view, surface),
            "visible leaf failed the public culling predicate");
    }
    wgen::tests::require(
        !wgen::isPlanetPatchVisible(
            {wgen::CubeSphereFace::Bottom, 0, 0, 0},
            view,
            surface),
        "far-side root should be horizon culled");
    wgen::tests::require(
        wgen::isPlanetPatchVisible(
            {wgen::CubeSphereFace::Bottom, 0, 0, 0},
            view,
            {.radius = 1.0, .minimumDisplacement = -1.0, .maximumDisplacement = 0.01}),
        "horizon culling should use the conservative minimum displacement bound");
}

void testOmittedDetailErrorContributesToScreenError() {
    const wgen::PlanetLodView view = makeView(3.0);
    const wgen::PlanetPatchId patch{wgen::CubeSphereFace::Top, 0, 0, 0};
    wgen::PlanetLodSurface surface{
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    };
    const wgen::PlanetPatchWorldError curvatureOnly =
        wgen::planetPatchWorldError(patch, surface);
    const double curvatureScreenError =
        wgen::planetPatchScreenErrorPixels(patch, view, surface);
    wgen::tests::require(
        curvatureOnly.sphereCurvature > 0.0 &&
            curvatureOnly.omittedTerrainDetail == 0.0 &&
            curvatureOnly.cachedData == 0.0,
        "sphere curvature should be an explicit world-error component");
    surface.maximumOmittedDetailError[0] = 0.5;
    const double withOmittedDetail = wgen::planetPatchScreenErrorPixels(patch, view, surface);

    wgen::tests::require(
        withOmittedDetail > curvatureScreenError,
        "omitted terrain detail should increase patch screen-space error");

    const wgen::PlanetPatchWorldError cachedError =
        wgen::planetPatchWorldError(patch, surface, 32, 0.75);
    wgen::tests::require(
        cachedError.maximum() == cachedError.cachedData &&
            cachedError.cachedData == 0.75,
        "known cached data should participate in the maximum world error");
    wgen::PlanetPatchErrorCache errorCache;
    errorCache.emplace(patch, 0.75);
    wgen::PlanetLodConfig config;
    config.maximumLevel = 1;
    config.selectedLeafBudget = 9;
    config.targetErrorPixels = std::midpoint(
        withOmittedDetail,
        wgen::planetPatchScreenErrorPixels(patch, view, surface, 32, 0.75));
    const wgen::PlanetLodSelection withoutCache =
        wgen::PlanetLodSelector{}.select(view, surface, config);
    const wgen::PlanetLodSelection withCache =
        wgen::PlanetLodSelector{}.select(view, surface, config, {}, errorCache);
    wgen::tests::require(
        std::find(withoutCache.leaves.begin(), withoutCache.leaves.end(), patch) !=
            withoutCache.leaves.end(),
        "the patch should remain coarse without a cached error");
    wgen::tests::require(
        std::find(withCache.leaves.begin(), withCache.leaves.end(), patch) ==
            withCache.leaves.end(),
        "a known cached error above the split threshold should refine the patch");
}

void testValidation() {
    wgen::PlanetLodConfig config;
    config.maximumLevel = wgen::MAX_PLANET_PATCH_LEVEL + 1;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&config] { wgen::validatePlanetLodConfig(config); },
        "selector should reject an invalid maximum level");

    config = {};
    config.selectedLeafBudget = 5;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&config] { wgen::validatePlanetLodConfig(config); },
        "selector should reject a budget smaller than the roots");

    wgen::PlanetLodView view = makeView();
    view.viewportHeight = 0;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&view] { wgen::validatePlanetLodView(view); },
        "selector should reject a zero-height viewport");

    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::validatePlanetLodSurface({
                .radius = 1.0,
                .maximumDisplacement = -0.1,
            });
        },
        "selector should reject negative terrain displacement");

    wgen::PlanetLodSurface invalidError{};
    invalidError.maximumOmittedDetailError[0] = -0.1;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&invalidError] { wgen::validatePlanetLodSurface(invalidError); },
        "selector should reject negative omitted-detail error");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&invalidError] {
            wgen::planetPatchWorldError(
                {wgen::CubeSphereFace::Top, 0, 0, 0},
                {},
                32,
                -0.1);
        },
        "selector should reject a negative cached data error");
}

void testCameraLodState() {
    lve::Camera3d camera;
    constexpr float verticalFov = 0.9F;
    constexpr float aspectRatio = 16.0F / 9.0F;
    constexpr float nearPlane = 0.1F;
    constexpr float farPlane = 50.0F;
    const glm::vec3 position{2.0F, -1.0F, 3.0F};
    const glm::vec3 target{-1.0F, 0.5F, -2.0F};
    const glm::vec3 suppliedUp{0.0F, 1.0F, 0.0F};

    camera.setPerspectiveProjection(verticalFov, aspectRatio, nearPlane, farPlane);
    camera.setViewTarget(position, target, suppliedUp);

    const glm::vec3 expectedForward = glm::normalize(target - position);
    wgen::tests::require(camera.position() == position, "camera should retain its LOD position");
    wgen::tests::expectNear(
        glm::dot(camera.forward(), expectedForward),
        1.0F,
        0.00001F,
        "camera should expose its normalized forward direction");
    wgen::tests::expectNear(glm::length(camera.right()), 1.0F, 0.00001F, "camera right must be normalized");
    wgen::tests::expectNear(glm::length(camera.up()), 1.0F, 0.00001F, "camera up must be normalized");
    wgen::tests::expectNear(glm::dot(camera.forward(), camera.right()), 0.0F, 0.00001F, "camera forward and right must be orthogonal");
    wgen::tests::expectNear(glm::dot(camera.forward(), camera.up()), 0.0F, 0.00001F, "camera forward and up must be orthogonal");
    wgen::tests::expectNear(glm::dot(camera.right(), camera.up()), 0.0F, 0.00001F, "camera right and up must be orthogonal");
    wgen::tests::expectNear(camera.verticalFov(), verticalFov, 0.0F, "camera should retain its vertical field of view");
    wgen::tests::expectNear(camera.aspectRatio(), aspectRatio, 0.0F, "camera should retain its aspect ratio");
    wgen::tests::expectNear(camera.nearPlane(), nearPlane, 0.0F, "camera should retain its near plane");
    wgen::tests::expectNear(camera.farPlane(), farPlane, 0.0F, "camera should retain its far plane");

    const glm::dvec3 precisePosition{1.0 + 0.0000000001, 2.0, 3.0};
    const glm::dvec3 preciseTarget{-0.25, 0.5, -0.75};
    camera.setGlobalViewTarget(
        precisePosition,
        preciseTarget,
        {0.0, 1.0, 0.0});
    wgen::tests::require(
        camera.globalPosition() == precisePosition,
        "camera should retain sub-float global positions for LOD selection");
    wgen::tests::require(
        std::abs(glm::length(camera.globalForward()) - 1.0) <=
            0.000000000000001,
        "camera global basis should be derived in double precision");

    wgen::tests::requireThrows<std::invalid_argument>(
        [&camera] { camera.setPerspectiveProjection(0.9F, 0.0F, 0.1F, 50.0F); },
        "camera should reject a non-positive aspect ratio");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&camera] { camera.setViewTarget({}, {}); },
        "camera should reject a zero view direction");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&camera] { camera.setViewTarget({}, {0.0F, 1.0F, 0.0F}, {0.0F, 2.0F, 0.0F}); },
        "camera should reject an up vector parallel to its view direction");
}

void testCameraRenderOriginRebase() {
    lve::Camera3d camera;
    camera.setPerspectiveProjection(0.9F, 16.0F / 9.0F, 0.001F, 20.0F);
    const glm::dvec3 cameraPosition{0.0000000001, 0.0, 3.0};
    camera.setGlobalViewTarget(cameraPosition, {}, {0.0, 1.0, 0.0});

    const glm::dvec3 globalPoint = cameraPosition +
        camera.globalForward() * 0.00000025 +
        camera.globalRight() * 0.00000005;
    camera.rebaseRenderOrigin(cameraPosition);
    const glm::vec4 firstViewPosition = camera.renderView() * glm::vec4{
        camera.positionRelativeToRenderOrigin(globalPoint),
        1.0F};

    const wgen::PlanetLodView view{
        .position = camera.globalPosition(),
        .forward = camera.globalForward(),
        .right = camera.globalRight(),
        .up = camera.globalUp(),
        .verticalFov = camera.verticalFov(),
        .aspectRatio = camera.aspectRatio(),
        .nearPlane = camera.nearPlane(),
        .farPlane = camera.farPlane(),
        .viewportHeight = 720,
    };
    wgen::PlanetLodConfig config;
    config.maximumLevel = 5;
    config.selectedLeafBudget = 128;
    const wgen::PlanetLodSurface surface{
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    };
    const wgen::PlanetLodSelection beforeRebase =
        wgen::PlanetLodSelector{}.select(view, surface, config);

    camera.rebaseRenderOrigin(cameraPosition + glm::dvec3{0.125, -0.25, 0.5});
    const glm::vec4 secondViewPosition = camera.renderView() * glm::vec4{
        camera.positionRelativeToRenderOrigin(globalPoint),
        1.0F};
    wgen::tests::require(
        glm::length(glm::vec3{firstViewPosition - secondViewPosition}) <= 0.0000001F,
        "rebasing should preserve camera-relative view coordinates");
    wgen::tests::require(
        camera.globalPosition() == cameraPosition,
        "rebasing should not change the global camera position");

    wgen::PlanetLodView afterView = view;
    afterView.position = camera.globalPosition();
    afterView.forward = camera.globalForward();
    afterView.right = camera.globalRight();
    afterView.up = camera.globalUp();
    const wgen::PlanetLodSelection afterRebase =
        wgen::PlanetLodSelector{}.select(afterView, surface, config);
    wgen::tests::require(
        beforeRebase.leaves == afterRebase.leaves &&
            beforeRebase.visibleLeaves == afterRebase.visibleLeaves,
        "render-origin rebasing should not change selected patch IDs");

    wgen::tests::requireThrows<std::invalid_argument>(
        [&camera] {
            camera.rebaseRenderOrigin({
                std::numeric_limits<double>::quiet_NaN(),
                0.0,
                0.0});
        },
        "camera should reject a non-finite render origin");
}

void testPlanetCameraDepthPolicies() {
    constexpr double PLANET_RADIUS = 6'371'000.0;
    const lve::PlanetCameraDepthRange local =
        lve::planetCameraDepthRange(100.0, PLANET_RADIUS);
    const lve::PlanetCameraDepthRange transition =
        lve::planetCameraDepthRange(0.05 * PLANET_RADIUS, PLANET_RADIUS);
    const lve::PlanetCameraDepthRange orbital =
        lve::planetCameraDepthRange(7.0 * PLANET_RADIUS, PLANET_RADIUS);

    wgen::tests::require(
        local.regime == lve::PlanetCameraDepthRegime::Local &&
            transition.regime == lve::PlanetCameraDepthRegime::Transition &&
            orbital.regime == lve::PlanetCameraDepthRegime::Orbital,
        "planet depth policy should identify local, transition, and orbital ranges");
    wgen::tests::require(
        local.nearPlane > 0.0F && local.farPlane > local.nearPlane,
        "local depth range should be valid");
    wgen::tests::require(
        orbital.nearPlane > local.nearPlane &&
            orbital.farPlane > 7.0F + 2.0F,
        "orbital depth range should preserve precision and cover the full planet");
    wgen::tests::require(
        transition.nearPlane > local.nearPlane &&
            transition.nearPlane < orbital.nearPlane &&
            transition.farPlane > transition.nearPlane,
        "transition depth range should blend between the two policies");

    wgen::tests::requireThrows<std::invalid_argument>(
        [] { lve::planetCameraDepthRange(-1.0, PLANET_RADIUS); },
        "planet depth policy should reject negative altitude");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { lve::planetCameraDepthRange(1.0, 0.0); },
        "planet depth policy should reject a non-positive radius");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest(
            "selection coverage balance and budget",
            testSelectionCoverageBalanceAndBudget);
        wgen::tests::runTest("selection hysteresis", testSelectionHysteresis);
        wgen::tests::runTest("culling and determinism", testCullingAndDeterminism);
        wgen::tests::runTest(
            "omitted detail screen error",
            testOmittedDetailErrorContributesToScreenError);
        wgen::tests::runTest("LOD validation", testValidation);
        wgen::tests::runTest("camera LOD state", testCameraLodState);
        wgen::tests::runTest(
            "camera render-origin rebase",
            testCameraRenderOriginRebase);
        wgen::tests::runTest(
            "planet camera depth policies",
            testPlanetCameraDepthPolicies);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
