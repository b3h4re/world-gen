#include "terrain/planet/planet_lod_coordinator.hpp"

#include "helpers.hpp"

#include <array>
#include <iostream>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <unordered_set>

namespace {

wgen::PlanetLodView makeView(double distance) {
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

wgen::PlanetLodView makeOrbitView(glm::dvec3 position) {
    const glm::dvec3 forward = glm::normalize(-position);
    const glm::dvec3 right = glm::normalize(glm::cross(forward, glm::dvec3{0.0, 1.0, 0.0}));
    const glm::dvec3 up = glm::cross(right, forward);
    return {
        .position = position,
        .forward = forward,
        .right = right,
        .up = up,
        .verticalFov = 50.0 * std::numbers::pi / 180.0,
        .aspectRatio = 16.0 / 9.0,
        .nearPlane = 0.1,
        .farPlane = 20.0,
        .viewportHeight = 720,
    };
}

std::vector<wgen::PlanetPatchId> roots() {
    std::vector<wgen::PlanetPatchId> result;
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        result.push_back({face, 0, 0, 0});
    }
    return result;
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
        const std::unordered_set<wgen::PlanetPatchId, wgen::PlanetPatchIdHash>& leaves) {
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

void requireValidActivePartition(const std::vector<wgen::PlanetPatchId>& leaves) {
    wgen::tests::require(!leaves.empty(), "active LOD partition should not be empty");
    for (std::size_t i = 0; i < leaves.size(); ++i) {
        for (std::size_t j = i + 1; j < leaves.size(); ++j) {
            wgen::tests::require(
                !isDescendantOrSelf(leaves[i], leaves[j]) &&
                    !isDescendantOrSelf(leaves[j], leaves[i]),
                "active LOD partition contains overlapping leaves");
        }
    }

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
            const std::optional<wgen::PlanetPatchId> neighbor = coveringLeaf(
                wgen::sameLevelNeighbor(leaf, edge).id,
                leafSet);
            if (neighbor) {
                wgen::tests::require(
                    neighbor->level + 1 >= leaf.level,
                    "active LOD partition is not balanced");
            }
        }
    }
}

void publishPlan(
        wgen::PlanetLodCoordinator& coordinator,
        const wgen::PlanetLodPatchPlan& plan) {
    coordinator.beginPatchPlan(plan);
    coordinator.publishPatchChanges(plan.upserts, plan.removals);
}

void testCoveragePreservingStreaming() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.maximumLevel = 4;
    lodConfig.targetErrorPixels = 0.5;
    lodConfig.selectedLeafBudget = 56;
    wgen::PlanetLodCoordinator coordinator{
        lodConfig,
        {.residentPatchBudget = 64, .patchUploadBudget = 8},
    };
    coordinator.setSurface({.radius = 1.0, .maximumDisplacement = 0.01});
    const std::vector<wgen::PlanetPatchId> rootIds = roots();
    coordinator.publishPatchChanges(rootIds, {});
    coordinator.updateSelection(makeView(1.3));

    wgen::tests::require(coordinator.activeLeaves() == rootIds, "bootstrap should draw the six roots");
    for (std::size_t iteration = 0;
            iteration < 100 && coordinator.activeLeaves() != coordinator.desiredLeaves();
            ++iteration) {
        const wgen::PlanetLodPatchPlan plan = coordinator.makePatchPlan();
        wgen::tests::require(!plan.upserts.empty(), "streaming stalled before reaching desired leaves");
        wgen::tests::require(plan.upserts.size() <= 8, "streaming exceeded the upload budget");
        publishPlan(coordinator, plan);
        requireValidActivePartition(coordinator.activeLeaves());
        wgen::tests::require(
            coordinator.residentIds().size() <= 64,
            "streaming exceeded the resident budget");
    }
    wgen::tests::require(
        coordinator.activeLeaves() == coordinator.desiredLeaves(),
        "streaming did not converge to the desired partition");

    coordinator.updateSelection(makeView(8.0));
    for (std::size_t iteration = 0;
            iteration < 100 && coordinator.activeLeaves() != coordinator.desiredLeaves();
            ++iteration) {
        const wgen::PlanetLodPatchPlan plan = coordinator.makePatchPlan();
        if (plan.upserts.empty()) {
            break;
        }
        publishPlan(coordinator, plan);
        requireValidActivePartition(coordinator.activeLeaves());
    }
    wgen::tests::require(
        coordinator.activeLeaves() == coordinator.desiredLeaves(),
        "coarsening did not converge without coverage gaps");
}

void testPendingRequestsAndVisibility() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.maximumLevel = 3;
    lodConfig.targetErrorPixels = 0.5;
    lodConfig.selectedLeafBudget = 24;
    wgen::PlanetLodCoordinator coordinator{
        lodConfig,
        {.residentPatchBudget = 32, .patchUploadBudget = 8},
    };
    coordinator.setSurface({.radius = 1.0, .maximumDisplacement = 0.01});
    const std::vector<wgen::PlanetPatchId> rootIds = roots();
    coordinator.publishPatchChanges(rootIds, {});
    coordinator.updateSelection(makeView(1.5));

    const wgen::PlanetLodPatchPlan first = coordinator.makePatchPlan();
    wgen::tests::require(!first.upserts.empty(), "near view should request refinement patches");
    coordinator.beginPatchPlan(first);
    wgen::tests::require(
        coordinator.makePatchPlan().upserts.empty(),
        "coordinator should not duplicate in-flight requests");
    coordinator.cancelPendingRequests();
    wgen::tests::require(
        coordinator.makePatchPlan().upserts == first.upserts,
        "cancelled requests should be eligible again deterministically");

    coordinator.updateVisibility(makeView(2.0));
    wgen::tests::require(
        coordinator.visibleActiveLeaves().size() < coordinator.activeLeaves().size(),
        "active draw visibility should apply culling independently of residency");
}

void testCacheTurnoverWhileOrbiting() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.maximumLevel = 4;
    lodConfig.targetErrorPixels = 0.5;
    lodConfig.selectedLeafBudget = 24;
    wgen::PlanetLodCoordinator coordinator{
        lodConfig,
        {.residentPatchBudget = 32, .patchUploadBudget = 8},
    };
    coordinator.setSurface({.radius = 1.0, .maximumDisplacement = 0.01});
    coordinator.publishPatchChanges(roots(), {});

    const auto converge = [&coordinator](const wgen::PlanetLodView& view) {
        coordinator.updateSelection(view);
        for (std::size_t iteration = 0;
                iteration < 100 && coordinator.activeLeaves() != coordinator.desiredLeaves();
                ++iteration) {
            const wgen::PlanetLodPatchPlan plan = coordinator.makePatchPlan();
            wgen::tests::require(!plan.upserts.empty(), "orbit streaming stalled before convergence");
            publishPlan(coordinator, plan);
            requireValidActivePartition(coordinator.activeLeaves());
            wgen::tests::require(
                coordinator.residentIds().size() <= coordinator.streamingConfig().residentPatchBudget,
                "orbit streaming exceeded the resident cache budget");
        }
        wgen::tests::require(
            coordinator.activeLeaves() == coordinator.desiredLeaves(),
            "orbit streaming did not converge to the desired partition");
    };

    converge(makeOrbitView({0.0, 0.0, 1.3}));
    const std::vector<wgen::PlanetPatchId> firstSide = coordinator.desiredLeaves();
    converge(makeOrbitView({0.0, 0.0, -1.3}));
    wgen::tests::require(
        coordinator.desiredLeaves() != firstSide,
        "orbiting to the opposite side should change the refined patch set");
}

void testStreamingValidation() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.selectedLeafBudget = 16;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&lodConfig] {
            wgen::PlanetLodCoordinator coordinator{
                lodConfig,
                {.residentPatchBudget = 20, .patchUploadBudget = 3},
            };
        },
        "streaming should reject a budget that cannot request four siblings");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("coverage-preserving streaming", testCoveragePreservingStreaming);
        wgen::tests::runTest("pending requests and visibility", testPendingRequestsAndVisibility);
        wgen::tests::runTest("cache turnover while orbiting", testCacheTurnoverWhileOrbiting);
        wgen::tests::runTest("streaming validation", testStreamingValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
