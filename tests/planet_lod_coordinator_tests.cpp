#include "terrain/planet/planet_lod_coordinator.hpp"

#include "helpers.hpp"

#include <algorithm>
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

void advanceCurrentTransitions(wgen::PlanetLodCoordinator& coordinator) {
    for (std::size_t iteration = 0;
            iteration < 32 && !coordinator.transitions().empty();
            ++iteration) {
        coordinator.advanceTransitions(coordinator.transitionConfig().durationSeconds);
    }
    wgen::tests::require(
        coordinator.transitions().empty(),
        "LOD transitions should settle deterministically");
}

void testCoveragePreservingStreaming() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.maximumLevel = 4;
    lodConfig.targetErrorPixels = 0.5;
    lodConfig.selectedLeafBudget = 56;
    wgen::PlanetLodCoordinator coordinator{
        lodConfig,
        {
            .residentPatchBudget = 64,
            .patchGenerationBudget = 8,
            .patchUploadBudget = 4,
            .maximumConcurrentPatchJobs = 1,
        },
    };
    coordinator.setSurface({
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    });
    const std::vector<wgen::PlanetPatchId> rootIds = roots();
    coordinator.publishPatchChanges(rootIds, {});
    coordinator.updateSelection(makeView(1.3));

    wgen::tests::require(coordinator.activeLeaves() == rootIds, "bootstrap should draw the six roots");
    for (std::size_t iteration = 0;
            iteration < 100 && coordinator.activeLeaves() != coordinator.desiredLeaves();
            ++iteration) {
        const wgen::PlanetLodPatchPlan plan = coordinator.makePatchPlan();
        wgen::tests::require(!plan.upserts.empty(), "streaming stalled before reaching desired leaves");
        wgen::tests::require(plan.upserts.size() <= 8, "streaming exceeded the generation budget");
        publishPlan(coordinator, plan);
        advanceCurrentTransitions(coordinator);
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
        if (!plan.upserts.empty()) {
            publishPlan(coordinator, plan);
        }
        wgen::tests::require(
            !plan.upserts.empty() || !coordinator.transitions().empty(),
            "coarsening stalled before reaching desired leaves");
        advanceCurrentTransitions(coordinator);
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
        {
            .residentPatchBudget = 32,
            .patchGenerationBudget = 8,
            .patchUploadBudget = 4,
            .maximumConcurrentPatchJobs = 1,
        },
    };
    coordinator.setSurface({
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    });
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
        {
            .residentPatchBudget = 32,
            .patchGenerationBudget = 8,
            .patchUploadBudget = 4,
            .maximumConcurrentPatchJobs = 1,
        },
    };
    coordinator.setSurface({
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    });
    coordinator.publishPatchChanges(roots(), {});

    const auto converge = [&coordinator](const wgen::PlanetLodView& view) {
        coordinator.updateSelection(view);
        for (std::size_t iteration = 0;
                iteration < 100 && coordinator.activeLeaves() != coordinator.desiredLeaves();
                ++iteration) {
            const wgen::PlanetLodPatchPlan plan = coordinator.makePatchPlan();
            wgen::tests::require(!plan.upserts.empty(), "orbit streaming stalled before convergence");
            publishPlan(coordinator, plan);
            advanceCurrentTransitions(coordinator);
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

void testCrossFaceStitchMasks() {
    constexpr std::array EDGES{
        wgen::PlanetPatchEdge::UMin,
        wgen::PlanetPatchEdge::UMax,
        wgen::PlanetPatchEdge::VMin,
        wgen::PlanetPatchEdge::VMax,
    };
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (const wgen::PlanetPatchEdge edge : EDGES) {
            wgen::PlanetPatchId fine{face, 1, 0, 0};
            switch (edge) {
                case wgen::PlanetPatchEdge::UMin: fine.x = 0; break;
                case wgen::PlanetPatchEdge::UMax: fine.x = 1; break;
                case wgen::PlanetPatchEdge::VMin: fine.y = 0; break;
                case wgen::PlanetPatchEdge::VMax: fine.y = 1; break;
            }
            const wgen::PlanetPatchNeighbor neighbor =
                wgen::sameLevelNeighbor(fine, edge);
            wgen::tests::require(
                neighbor.id.face != fine.face,
                "test patch should exercise a cube-face boundary");
            const wgen::PlanetPatchId coarse = *wgen::parent(neighbor.id);
            const std::array active{fine, coarse};
            const wgen::PlanetPatchEdgeMask mask = wgen::planetPatchStitchMask(
                fine,
                active);
            wgen::tests::require(
                (mask & wgen::planetPatchEdgeBit(edge)) != 0,
                "fine patches should stitch to coarser neighbors across cube faces");
        }
    }
}

wgen::PlanetLodCoordinator makeTransitionCoordinator() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.maximumLevel = 1;
    lodConfig.targetErrorPixels = 20.0;
    lodConfig.selectedLeafBudget = 9;
    wgen::PlanetLodCoordinator coordinator{
        lodConfig,
        {
            .residentPatchBudget = 16,
            .patchGenerationBudget = 4,
            .patchUploadBudget = 2,
            .maximumConcurrentPatchJobs = 1,
        },
        {.durationSeconds = 2.0, .debugTimeScale = 1.0},
    };
    coordinator.setSurface({
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    });
    coordinator.publishPatchChanges(roots(), {});
    return coordinator;
}

void testSplitAndMergeTransitions() {
    wgen::PlanetLodCoordinator coordinator = makeTransitionCoordinator();
    coordinator.updateSelection(makeView(1.3));
    const wgen::PlanetLodPatchPlan splitPlan = coordinator.makePatchPlan();
    wgen::tests::require(
        splitPlan.upserts.size() == 4,
        "a balanced split should request all four siblings together");
    publishPlan(coordinator, splitPlan);

    wgen::tests::require(
        coordinator.transitions().size() == 1,
        "resident children should start exactly one split transition");
    const wgen::PlanetPatchId parentId = coordinator.transitions().front().parent;
    wgen::tests::require(
        coordinator.transitions().front().kind == wgen::PlanetLodTransitionKind::Split &&
            coordinator.transitions().front().childMorph == 0.0,
        "a split should begin at the parent representation");
    wgen::tests::require(
        coordinator.residentIds().contains(parentId),
        "the parent should remain resident while its children morph in");
    bool foundVisibleTransitionChild = false;
    for (const wgen::PlanetPatchDrawState& draw : coordinator.visibleDrawStates()) {
        const std::optional<wgen::PlanetPatchId> drawParent = wgen::parent(draw.id);
        if (drawParent && *drawParent == parentId) {
            foundVisibleTransitionChild = true;
            wgen::tests::expectNear(
                draw.morph,
                0.0F,
                0.00001F,
                "draw state should expose the transition morph independently of residency");
        }
    }
    wgen::tests::require(
        foundVisibleTransitionChild,
        "the visible split should publish child draw states");
    for (const wgen::PlanetPatchId& childId : wgen::children(parentId)) {
        wgen::tests::require(
            std::find(
                coordinator.activeLeaves().begin(),
                coordinator.activeLeaves().end(),
                childId) != coordinator.activeLeaves().end(),
            "children should be the only authoritative split surface");
    }

    coordinator.advanceTransitions(1.0);
    wgen::tests::expectNear(
        static_cast<float>(coordinator.transitions().front().childMorph),
        0.5F,
        0.00001F,
        "split morph should advance with elapsed time");
    coordinator.advanceTransitions(1.0);
    wgen::tests::require(
        coordinator.transitions().empty(),
        "split transition should finish at the fine representation");

    coordinator.updateSelection(makeView(8.0));
    wgen::tests::require(
        coordinator.transitions().size() == 1 &&
            coordinator.transitions().front().kind == wgen::PlanetLodTransitionKind::Merge &&
            coordinator.transitions().front().childMorph == 1.0,
        "coarsening should begin by morphing authoritative children backward");
    coordinator.advanceTransitions(1.0);
    wgen::tests::expectNear(
        static_cast<float>(coordinator.transitions().front().childMorph),
        0.5F,
        0.00001F,
        "merge morph should reverse toward the parent representation");
    coordinator.advanceTransitions(1.0);
    wgen::tests::require(
        coordinator.transitions().empty(),
        "merge transition should finish deterministically");
    wgen::tests::require(
        std::find(
            coordinator.activeLeaves().begin(),
            coordinator.activeLeaves().end(),
            parentId) != coordinator.activeLeaves().end(),
        "the parent should become authoritative only after the merge completes");
}

void testTransitionReversalAndSlowMotion() {
    wgen::PlanetLodCoordinator coordinator = makeTransitionCoordinator();
    coordinator.updateSelection(makeView(1.3));
    publishPlan(coordinator, coordinator.makePatchPlan());
    coordinator.advanceTransitions(0.5);
    wgen::tests::expectNear(
        static_cast<float>(coordinator.transitions().front().childMorph),
        0.25F,
        0.00001F,
        "split should reach a deterministic partial morph");

    coordinator.setTransitionDebugTimeScale(0.0);
    coordinator.advanceTransitions(100.0);
    wgen::tests::expectNear(
        static_cast<float>(coordinator.transitions().front().childMorph),
        0.25F,
        0.00001F,
        "zero debug speed should pause transitions");
    coordinator.setTransitionDebugTimeScale(0.5);
    coordinator.updateSelection(makeView(8.0));
    wgen::tests::require(
        coordinator.transitions().front().kind == wgen::PlanetLodTransitionKind::Merge,
        "changing direction should reverse an in-progress split");
    wgen::tests::expectNear(
        static_cast<float>(coordinator.transitions().front().childMorph),
        0.25F,
        0.00001F,
        "reversing should preserve continuous morph progress");
    coordinator.advanceTransitions(1.0);
    wgen::tests::require(
        coordinator.transitions().empty(),
        "a reversed transition should complete without a second discontinuity");
}

void testPrioritizedIndependentBudgets() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.maximumLevel = 3;
    lodConfig.targetErrorPixels = 0.0001;
    lodConfig.selectedLeafBudget = 24;
    wgen::PlanetLodCoordinator coordinator{
        lodConfig,
        {
            .residentPatchBudget = 40,
            .patchGenerationBudget = 8,
            .patchUploadBudget = 2,
            .maximumConcurrentPatchJobs = 2,
        },
    };
    const wgen::PlanetLodSurface surface{
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    };
    const wgen::PlanetLodView view = makeView(1.3);
    coordinator.setSurface(surface);
    coordinator.publishPatchChanges(roots(), {});
    coordinator.updateVisibility(makeView(1.5));
    coordinator.updateVisibility(view);
    coordinator.updateSelection(view);

    const wgen::PlanetLodPatchPlan plan = coordinator.makePatchPlan();
    wgen::tests::require(
        plan.upserts.size() == 8,
        "CPU generation should fill its own budget independently of GPU uploads");
    wgen::tests::require(
        plan.upserts.size() > coordinator.streamingConfig().patchUploadBudget,
        "the generation plan should not be capped by the per-frame upload budget");
    const wgen::PlanetPatchId firstAnchor = *wgen::parent(plan.upserts.front());
    const wgen::PlanetPatchId secondAnchor = *wgen::parent(plan.upserts[4]);
    for (std::size_t index = 0; index < 4; ++index) {
        wgen::tests::require(
            wgen::parent(plan.upserts[index]) == firstAnchor,
            "the highest-priority sibling group should remain contiguous");
        wgen::tests::require(
            wgen::parent(plan.upserts[index + 4]) == secondAnchor,
            "the second-priority sibling group should remain contiguous");
    }
    wgen::tests::require(
        wgen::planetPatchScreenErrorPixels(firstAnchor, view, surface) >=
            wgen::planetPatchScreenErrorPixels(secondAnchor, view, surface),
        "visible generation groups should be ordered by descending screen error");
}

void testRevisionCancellationStress() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.maximumLevel = 4;
    lodConfig.targetErrorPixels = 0.0001;
    lodConfig.selectedLeafBudget = 24;
    wgen::PlanetLodCoordinator coordinator{
        lodConfig,
        {
            .residentPatchBudget = 40,
            .patchGenerationBudget = 4,
            .patchUploadBudget = 2,
            .maximumConcurrentPatchJobs = 2,
        },
    };
    coordinator.setSurface({
        .radius = 1.0,
        .minimumDisplacement = -0.01,
        .maximumDisplacement = 0.01,
    });
    const std::vector<wgen::PlanetPatchId> rootIds = roots();
    coordinator.publishPatchChanges(rootIds, {});
    coordinator.updateSelection(makeView(1.3));

    const wgen::PlanetLodPatchPlan first = coordinator.makePatchPlan();
    coordinator.beginPatchPlan(first, 10);
    const wgen::PlanetLodPatchPlan second = coordinator.makePatchPlan();
    coordinator.beginPatchPlan(second, 10);
    wgen::tests::require(
        coordinator.pendingPatchCount() == 8,
        "two generation jobs should reserve exactly the bounded pending capacity");
    wgen::tests::require(
        coordinator.makePatchPlan().upserts.empty(),
        "the coordinator should not create unbounded work after filling the queue");

    coordinator.cancelPendingRequestsBefore(11);
    wgen::tests::require(
        coordinator.pendingPatchCount() == 0,
        "a newer revision should release stale work reservations immediately");
    wgen::tests::require(
        !coordinator.publishPatchChanges(first.upserts, rootIds, 10),
        "a stale completion should be rejected by revision");
    wgen::tests::require(
        coordinator.activeLeaves() == rootIds,
        "a stale completion must not remove current fallback coverage");
    for (const wgen::PlanetPatchId& root : rootIds) {
        wgen::tests::require(
            coordinator.residentIds().contains(root),
            "a stale removal must preserve every resident fallback root");
    }

    std::uint64_t revision = 12;
    for (std::size_t iteration = 0; iteration < 64; ++iteration, ++revision) {
        const double side = iteration % 2 == 0 ? 1.0 : -1.0;
        const double distance = iteration % 4 < 2 ? 1.3 : 8.0;
        coordinator.updateSelection(makeOrbitView({0.0, 0.0, side * distance}));
        coordinator.cancelPendingRequestsBefore(revision);
        const wgen::PlanetLodPatchPlan plan = coordinator.makePatchPlan();
        if (!plan.upserts.empty() || !plan.removals.empty()) {
            coordinator.beginPatchPlan(plan, revision);
        }
        wgen::tests::require(
            coordinator.pendingPatchCount() <= 8,
            "rapid zoom and direction reversal exceeded the bounded pending-work budget");
        requireValidActivePartition(coordinator.activeLeaves());
    }
    coordinator.updateSelection(makeView(1.3));
    coordinator.cancelPendingRequestsBefore(revision);
    const wgen::PlanetLodPatchPlan regenerationPending =
        coordinator.makePatchPlan();
    wgen::tests::require(
        !regenerationPending.upserts.empty(),
        "regeneration stress should begin with patch work pending");
    coordinator.beginPatchPlan(regenerationPending, revision);
    coordinator.cancelPendingRequestsBefore(++revision);
    wgen::tests::require(
        coordinator.pendingPatchCount() == 0,
        "simulated regeneration should invalidate patch work without interruption");
    wgen::tests::require(
        !coordinator.publishPatchChanges(
            regenerationPending.upserts,
            {},
            revision - 1),
        "patch work completing after regeneration should remain stale");
    wgen::tests::require(
        coordinator.activeLeaves() == rootIds,
        "regeneration cancellation should retain the visible root fallback");
}

void testKnownPatchErrorPublication() {
    wgen::PlanetLodCoordinator coordinator;
    const wgen::PlanetPatchId root{wgen::CubeSphereFace::Top, 0, 0, 0};
    const std::array samples{
        wgen::PlanetPatchErrorSample{root, 0.125},
    };
    coordinator.publishPatchChanges({}, {}, 0, samples);
    wgen::tests::require(
        coordinator.knownPatchErrors().at(root) == 0.125,
        "published cached geometry errors should feed future selection");
}

void testStreamingValidation() {
    wgen::PlanetLodConfig lodConfig;
    lodConfig.selectedLeafBudget = 16;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&lodConfig] {
            wgen::PlanetLodCoordinator coordinator{
                lodConfig,
                {
                    .residentPatchBudget = 20,
                    .patchGenerationBudget = 3,
                    .patchUploadBudget = 2,
                    .maximumConcurrentPatchJobs = 1,
                },
            };
        },
        "streaming should reject a budget that cannot request four siblings");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&lodConfig] {
            wgen::PlanetLodCoordinator coordinator{
                lodConfig,
                {
                    .residentPatchBudget = 20,
                    .patchGenerationBudget = 4,
                    .patchUploadBudget = 1,
                    .maximumConcurrentPatchJobs = 0,
                },
            };
        },
        "streaming should reject an empty bounded job queue");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::validatePlanetLodTransitionConfig({.durationSeconds = 0.0}); },
        "transitions should reject a non-positive duration");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::validatePlanetLodTransitionConfig({
                .durationSeconds = 1.0,
                .debugTimeScale = -1.0,
            });
        },
        "transitions should reject a negative debug speed");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("coverage-preserving streaming", testCoveragePreservingStreaming);
        wgen::tests::runTest("pending requests and visibility", testPendingRequestsAndVisibility);
        wgen::tests::runTest("cache turnover while orbiting", testCacheTurnoverWhileOrbiting);
        wgen::tests::runTest("cross-face stitch masks", testCrossFaceStitchMasks);
        wgen::tests::runTest("split and merge transitions", testSplitAndMergeTransitions);
        wgen::tests::runTest("transition reversal and slow motion", testTransitionReversalAndSlowMotion);
        wgen::tests::runTest("prioritized independent budgets", testPrioritizedIndependentBudgets);
        wgen::tests::runTest("revision cancellation stress", testRevisionCancellationStress);
        wgen::tests::runTest("known patch error publication", testKnownPatchErrorPublication);
        wgen::tests::runTest("streaming validation", testStreamingValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
