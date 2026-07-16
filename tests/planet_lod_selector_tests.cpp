#include "terrain/planet/planet_lod_selector.hpp"

#include "game/camera/camera_3d.hpp"
#include "helpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
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
    const double spacingOnly = wgen::planetPatchScreenErrorPixels(patch, view, surface);
    surface.maximumOmittedDetailError[0] = 0.5;
    const double withOmittedDetail = wgen::planetPatchScreenErrorPixels(patch, view, surface);

    wgen::tests::require(
        withOmittedDetail > spacingOnly,
        "omitted terrain detail should increase patch screen-space error");
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
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
