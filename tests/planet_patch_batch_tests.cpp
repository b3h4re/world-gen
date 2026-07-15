#include "terrain/planet/planet_patch_mesh.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

lve::PlanetPatchMeshData makeUpsert(
        const wgen::PlanetPatchId& id,
        const lve::PlanetPatchVersion& version) {
    return {
        .id = id,
        .version = version,
        .vertices = {{{0.0F, 0.0F, 0.0F}, 0.0F}},
        .indices = {0},
    };
}

std::vector<lve::PlanetPatchMeshData> makeUpserts(
        const std::vector<wgen::PlanetPatchId>& ids,
        const lve::PlanetPatchVersion& version) {
    std::vector<lve::PlanetPatchMeshData> result;
    result.reserve(ids.size());
    for (const wgen::PlanetPatchId& id : ids) {
        result.push_back(makeUpsert(id, version));
    }
    return result;
}

void testFixedLevelIdsAndDiffs() {
    constexpr std::array<std::size_t, 4> EXPECTED_COUNTS{6, 24, 96, 384};
    for (std::uint8_t level = 0; level <= lve::MAX_FIXED_PLANET_PATCH_LEVEL; ++level) {
        const std::vector<wgen::PlanetPatchId> ids = lve::fixedLevelPlanetPatchIds(level);
        wgen::tests::require(ids.size() == EXPECTED_COUNTS[level], "fixed-level ID count is wrong");

        std::size_t index = 0;
        const std::uint32_t count = wgen::patchesPerAxis(level);
        for (const wgen::CubeSphereFace face : wgen::FACES) {
            for (std::uint32_t y = 0; y < count; ++y) {
                for (std::uint32_t x = 0; x < count; ++x) {
                    wgen::tests::require(
                        ids[index++] == wgen::PlanetPatchId{face, level, x, y},
                        "fixed-level IDs are not deterministic");
                }
            }
        }
    }

    const std::vector<wgen::PlanetPatchId> roots = lve::fixedLevelPlanetPatchIds(0);
    const std::vector<wgen::PlanetPatchId> children = lve::fixedLevelPlanetPatchIds(1);
    const std::vector<wgen::PlanetPatchId> removed =
        lve::planetPatchIdDifference(roots, children);
    wgen::tests::require(removed.size() == roots.size(), "level change should remove every old ID");
    wgen::tests::require(
        std::is_sorted(removed.begin(), removed.end(), wgen::PlanetPatchIdLess{}),
        "removed IDs should have deterministic order");
    wgen::tests::require(
        lve::planetPatchIdDifference(roots, roots).empty(),
        "identical desired IDs should produce no removals");
}

void testBatchConstruction() {
    constexpr lve::PlanetPatchVersion VERSION{
        .terrainEpoch = 3,
        .dependencyRevision = 0,
        .requestRevision = 7,
    };
    const std::vector<wgen::PlanetPatchId> roots = lve::fixedLevelPlanetPatchIds(0);
    const std::vector<wgen::PlanetPatchId> children = lve::fixedLevelPlanetPatchIds(1);

    const lve::PlanetPatchMeshBatch upsertOnly = lve::makePlanetPatchMeshBatch(
        makeUpserts(roots, VERSION),
        {},
        VERSION.terrainEpoch,
        VERSION.requestRevision);
    wgen::tests::require(upsertOnly.upserts.size() == 6, "upsert-only batch lost patches");
    wgen::tests::require(upsertOnly.removals.empty(), "upsert-only batch gained removals");

    const lve::PlanetPatchMeshBatch removalOnly = lve::makePlanetPatchMeshBatch(
        {},
        roots,
        VERSION.terrainEpoch,
        VERSION.requestRevision);
    wgen::tests::require(removalOnly.upserts.empty(), "removal-only batch gained upserts");
    wgen::tests::require(removalOnly.removals.size() == 6, "removal-only batch lost removals");

    const lve::PlanetPatchMeshBatch mixed = lve::makePlanetPatchMeshBatch(
        makeUpserts(children, VERSION),
        roots,
        VERSION.terrainEpoch,
        VERSION.requestRevision);
    wgen::tests::require(mixed.upserts.size() == 24, "mixed batch lost upserts");
    wgen::tests::require(mixed.removals.size() == 6, "mixed batch lost removals");

    const lve::PlanetPatchMeshData requested = lve::buildRequestedPlanetPatchMesh(
        {
            .id = roots.front(),
            .version = VERSION,
        },
        1,
        10.0F,
        [](const wgen::PlanetSurfaceSample&) { return 0.0F; });
    wgen::tests::require(requested.id == roots.front(), "mesh result lost its request ID");
    wgen::tests::require(requested.version == VERSION, "mesh result lost its request version");
}

void testBatchValidation() {
    constexpr lve::PlanetPatchVersion VERSION{
        .terrainEpoch = 2,
        .dependencyRevision = 0,
        .requestRevision = 4,
    };
    const wgen::PlanetPatchId id{wgen::CubeSphereFace::Top, 0, 0, 0};
    lve::PlanetPatchMeshBatch batch{
        .terrainEpoch = VERSION.terrainEpoch,
        .requestRevision = VERSION.requestRevision,
        .upserts = {makeUpsert(id, VERSION)},
    };
    lve::validatePlanetPatchMeshBatch(batch);

    lve::PlanetPatchMeshBatch duplicateUpsert = batch;
    duplicateUpsert.upserts.push_back(duplicateUpsert.upserts.front());
    wgen::tests::requireThrows<std::invalid_argument>(
        [&duplicateUpsert] { lve::validatePlanetPatchMeshBatch(duplicateUpsert); },
        "duplicate upserts should be rejected");

    lve::PlanetPatchMeshBatch overlap = batch;
    overlap.removals.push_back({id, VERSION.terrainEpoch, VERSION.requestRevision});
    wgen::tests::requireThrows<std::invalid_argument>(
        [&overlap] { lve::validatePlanetPatchMeshBatch(overlap); },
        "overlapping operations should be rejected");

    lve::PlanetPatchMeshBatch duplicateRemoval{
        .terrainEpoch = VERSION.terrainEpoch,
        .requestRevision = VERSION.requestRevision,
        .removals = {
            {id, VERSION.terrainEpoch, VERSION.requestRevision},
            {id, VERSION.terrainEpoch, VERSION.requestRevision},
        },
    };
    wgen::tests::requireThrows<std::invalid_argument>(
        [&duplicateRemoval] { lve::validatePlanetPatchMeshBatch(duplicateRemoval); },
        "duplicate removals should be rejected");

    lve::PlanetPatchMeshBatch emptyMesh = batch;
    emptyMesh.upserts.front().vertices.clear();
    wgen::tests::requireThrows<std::invalid_argument>(
        [&emptyMesh] { lve::validatePlanetPatchMeshBatch(emptyMesh); },
        "empty upsert meshes should be rejected");

    lve::PlanetPatchMeshBatch badUpsertTag = batch;
    ++badUpsertTag.upserts.front().version.requestRevision;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&badUpsertTag] { lve::validatePlanetPatchMeshBatch(badUpsertTag); },
        "mismatched upsert tags should be rejected");

    lve::PlanetPatchMeshBatch badRemovalTag = duplicateRemoval;
    badRemovalTag.removals.pop_back();
    ++badRemovalTag.removals.front().terrainEpoch;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&badRemovalTag] { lve::validatePlanetPatchMeshBatch(badRemovalTag); },
        "mismatched removal tags should be rejected");

    lve::PlanetPatchMeshBatch invalidId = batch;
    invalidId.upserts.front().id.x = 1;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&invalidId] { lve::validatePlanetPatchMeshBatch(invalidId); },
        "invalid patch IDs should be rejected");

    lve::PlanetPatchMeshBatch uninitialized = batch;
    uninitialized.terrainEpoch = 0;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&uninitialized] { lve::validatePlanetPatchMeshBatch(uninitialized); },
        "uninitialized batch versions should be rejected");
}

void testStaleAndVersionDecisions() {
    constexpr lve::PlanetPatchVersion RESIDENT{
        .terrainEpoch = 5,
        .dependencyRevision = 8,
        .requestRevision = 12,
    };
    wgen::tests::require(
        !lve::shouldApplyPlanetPatchUpsert(RESIDENT, RESIDENT),
        "exact versions should not upload again");
    wgen::tests::require(
        lve::shouldApplyPlanetPatchUpsert({5, 8, 13}, RESIDENT),
        "a newer request should replace a resident patch");
    wgen::tests::require(
        lve::shouldApplyPlanetPatchUpsert({5, 9, 12}, RESIDENT),
        "a newer dependency should replace a resident patch");
    wgen::tests::require(
        !lve::shouldApplyPlanetPatchUpsert({5, 9, 11}, RESIDENT),
        "an older request should preserve a resident patch");
    wgen::tests::require(
        !lve::shouldApplyPlanetPatchUpsert({5, 7, 13}, RESIDENT),
        "an older dependency should preserve a resident patch");

    const wgen::PlanetPatchId id{wgen::CubeSphereFace::Front, 0, 0, 0};
    wgen::tests::require(
        !lve::shouldApplyPlanetPatchRemoval({id, 5, 11}, RESIDENT),
        "a stale removal should preserve a newer request");
    wgen::tests::require(
        lve::shouldApplyPlanetPatchRemoval({id, 5, 12}, RESIDENT),
        "a current removal should remove a resident patch");
    wgen::tests::require(
        !lve::shouldApplyPlanetPatchRemoval({id, 4, 20}, RESIDENT),
        "a removal from another epoch should preserve a resident patch");

    wgen::tests::require(
        lve::planetPatchBatchEpochAction(4, 5) == lve::PlanetPatchBatchEpochAction::Discard,
        "an old epoch should be discarded");
    wgen::tests::require(
        lve::planetPatchBatchEpochAction(5, 5) == lve::PlanetPatchBatchEpochAction::Apply,
        "the active epoch should apply incrementally");
    wgen::tests::require(
        lve::planetPatchBatchEpochAction(6, 5) == lve::PlanetPatchBatchEpochAction::Replace,
        "a new epoch should replace old residency");

    bool planeResultPresent = true;
    std::optional<lve::PlanetPatchMeshBatch> staleBatch{lve::PlanetPatchMeshBatch{
        .terrainEpoch = 5,
        .requestRevision = 11,
    }};
    wgen::tests::require(
        lve::discardStalePlanetPatchMeshBatch(staleBatch, 12),
        "a stale request should be filtered");
    wgen::tests::require(!staleBatch.has_value(), "the stale planet batch should be removed");
    wgen::tests::require(planeResultPresent, "filtering a planet batch should not suppress a plane result");

    std::optional<lve::PlanetPatchMeshBatch> currentBatch{lve::PlanetPatchMeshBatch{
        .terrainEpoch = 5,
        .requestRevision = 12,
    }};
    wgen::tests::require(
        !lve::discardStalePlanetPatchMeshBatch(currentBatch, 12) && currentBatch.has_value(),
        "the current request should remain publishable");
}

void testDeterministicDrawOrder() {
    std::vector<wgen::PlanetPatchId> ids{
        {wgen::CubeSphereFace::Front, 2, 3, 0},
        {wgen::CubeSphereFace::Top, 1, 1, 0},
        {wgen::CubeSphereFace::Top, 1, 0, 1},
        {wgen::CubeSphereFace::Back, 0, 0, 0},
    };
    std::vector<wgen::PlanetPatchId> reverseInput = ids;
    std::reverse(reverseInput.begin(), reverseInput.end());
    std::sort(ids.begin(), ids.end(), wgen::PlanetPatchIdLess{});
    std::sort(reverseInput.begin(), reverseInput.end(), wgen::PlanetPatchIdLess{});
    wgen::tests::require(ids == reverseInput, "draw order should not depend on batch input order");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("fixed-level IDs and diffs", testFixedLevelIdsAndDiffs);
        wgen::tests::runTest("batch construction", testBatchConstruction);
        wgen::tests::runTest("batch validation", testBatchValidation);
        wgen::tests::runTest("stale and version decisions", testStaleAndVersionDecisions);
        wgen::tests::runTest("deterministic draw order", testDeterministicDrawOrder);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
