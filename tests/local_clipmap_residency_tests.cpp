#include "terrain/planet/local_clipmap_residency.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {

wgen::LocalClipmapConfig testClipmapConfig(std::uint32_t levels = 3) {
    return {
        .samplesPerAxis = 9,
        .levelCount = levels,
        .finestCellSpacingMeters = 10.0,
        .levelScale = 2,
        .overlapWidthCells = 1,
        .transitionWidthCells = 1,
    };
}

std::vector<wgen::LocalClipmapCacheOrigin> makeOrigins(
        const wgen::LocalClipmapConfig& config,
        std::uint64_t frameRevision,
        wgen::LocalClipmapGridCoordinate center = {}) {
    std::vector<wgen::LocalClipmapCacheOrigin> result;
    result.reserve(config.levelCount);
    for (std::uint32_t level = 0; level < config.levelCount; ++level) {
        result.push_back({{}, frameRevision, level, center});
    }
    return result;
}

float syntheticHeight(
        const wgen::LocalClipmapUpdateIdentity& identity,
        const wgen::LocalClipmapGridCoordinate& coordinate) {
    return static_cast<float>(
        static_cast<double>(identity.level) * 10'000.0 +
        static_cast<double>(coordinate.x) * 10.0 +
        static_cast<double>(coordinate.y));
}

wgen::LocalClipmapHeightUpdateResult makeResult(
        const wgen::LocalClipmapHeightUpdateRequest& request) {
    wgen::LocalClipmapHeightUpdateResult result{
        .identity = request.identity,
        .workId = request.workId,
        .generationLatencyMilliseconds = 0.25,
    };
    result.writes.reserve(request.coordinates.size());
    for (const wgen::LocalClipmapGridCoordinate& coordinate :
            request.coordinates) {
        result.writes.push_back({
            coordinate,
            syntheticHeight(request.identity, coordinate),
        });
    }
    return result;
}

std::size_t generationSampleCount(
        const std::vector<wgen::LocalClipmapHeightUpdateRequest>& requests) {
    return std::accumulate(
        requests.begin(),
        requests.end(),
        std::size_t{},
        [](std::size_t total, const auto& request) {
            return total + request.coordinates.size();
        });
}

std::size_t uploadSampleCount(
        const std::vector<wgen::LocalClipmapGpuUploadBatch>& batches) {
    return std::accumulate(
        batches.begin(),
        batches.end(),
        std::size_t{},
        [](std::size_t total, const auto& batch) {
            return total + batch.writes.size();
        });
}

void drainResidency(wgen::LocalClipmapHeightResidency& residency) {
    std::size_t iterations = 0;
    while (residency.pendingGenerationSampleCount() != 0 ||
            residency.pendingUploadSampleCount() != 0) {
        const auto requests = residency.takeGenerationRequestsForFrame();
        for (const auto& request : requests) {
            wgen::tests::require(
                residency.publishHeightUpdate(makeResult(request)),
                "current synthetic generation result should publish");
        }
        const auto uploads = residency.takeGpuUploadBatchesForFrame();
        for (const auto& upload : uploads) {
            wgen::tests::require(
                residency.commitGpuUploadBatch(upload),
                "current synthetic upload should commit");
        }
        wgen::tests::require(
            !requests.empty() || !uploads.empty(),
            "residency drain made no progress");
        wgen::tests::require(++iterations < 10'000, "residency drain did not converge");
    }
}

void testIdentityPriorityAndToroidalAddressing() {
    const wgen::LocalClipmapConfig config = testClipmapConfig();
    const wgen::LocalClipmapUpdateIdentity identity{
        .terrainEpoch = 4,
        .terrainDependencyRevision = 8,
        .localFrameRevision = 12,
        .level = 1,
        .snappedGridOrigin = {30, -20},
        .requestRevision = 16,
    };
    wgen::validateLocalClipmapUpdateIdentity(identity, config);
    wgen::tests::require(
        wgen::localClipmapResidencyPriority(0, 3) == 0 &&
            wgen::localClipmapResidencyPriority(2, 3) == 1 &&
            wgen::localClipmapResidencyPriority(1, 3) == 2,
        "residency should prioritize center, handoff, then intermediate levels");
    wgen::tests::require(
        wgen::localClipmapToroidalIndex({-1, -1}, 9) == 80 &&
            wgen::localClipmapToroidalIndex({8, 8}, 9) == 80 &&
            wgen::localClipmapToroidalIndex({17, 17}, 9) == 80,
        "toroidal addressing should wrap positive and negative coordinates");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::validateLocalClipmapResidencyConfig({
                .generationBudgetSamplesPerFrame = 0,
            });
        },
        "residency should reject an empty generation budget");
}

void testStrictBudgetsPriorityAndPartialValidity() {
    const wgen::LocalClipmapConfig config = testClipmapConfig();
    wgen::LocalClipmapHeightResidency residency{
        config,
        {
            .generationBudgetSamplesPerFrame = 10,
            .uploadBudgetSamplesPerFrame = 4,
        },
    };
    residency.requestResidency(1, 0, 1, makeOrigins(config, 1));

    const auto requests = residency.takeGenerationRequestsForFrame();
    wgen::tests::require(
        generationSampleCount(requests) == 10 && requests.size() == 1 &&
            requests.front().identity.level == 0,
        "the strict generation budget should serve the fine center first");
    wgen::tests::require(
        residency.publishHeightUpdate(makeResult(requests.front())),
        "current fine-center work should publish");
    wgen::tests::require(
        residency.desiredCpuHeight(0, {-4, -4}).has_value() &&
            !residency.desiredGpuHeight(0, {-4, -4}).has_value(),
        "CPU-ready samples must remain invalid until their GPU upload commits");

    const auto uploads = residency.takeGpuUploadBatchesForFrame();
    wgen::tests::require(
        uploadSampleCount(uploads) == 4 && uploads.size() == 1 &&
            uploads.front().identity.level == 0,
        "the strict upload budget should serve only four center samples");
    wgen::tests::require(
        residency.commitGpuUploadBatch(uploads.front()),
        "current partial upload should commit");
    const std::vector<wgen::LocalClipmapValidRegion> valid =
        residency.desiredValidRegions(0);
    wgen::tests::require(
        valid == std::vector<wgen::LocalClipmapValidRegion>{{{-4, -4}, 4, 1}},
        "validity should expose the exact uploaded strip and nothing else");
    wgen::tests::require(
        !residency.hasCompleteActiveLevel(0),
        "a partial replacement must not become active");
    wgen::tests::require(
        residency.metrics().samplesGenerated == 10 &&
            residency.metrics().samplesUploaded == 4 &&
            residency.metrics().bytesUploaded == 4 * sizeof(float),
        "residency metrics should count generated and uploaded samples");

    drainResidency(residency);
    for (std::uint32_t level = 0; level < config.levelCount; ++level) {
        wgen::tests::require(
            residency.hasCompleteActiveLevel(level) &&
                residency.activeValidRegions(level) ==
                    std::vector<wgen::LocalClipmapValidRegion>{{{-4, -4}, 9, 9}},
            "draining should produce one fully valid active cache per level");
    }
}

void testSmallMovementUpdatesOnlyExposedColumn() {
    const wgen::LocalClipmapConfig config = testClipmapConfig();
    wgen::LocalClipmapHeightResidency residency{
        config,
        {
            .generationBudgetSamplesPerFrame = 1'000,
            .uploadBudgetSamplesPerFrame = 1'000,
        },
    };
    std::vector<wgen::LocalClipmapCacheOrigin> origins = makeOrigins(config, 1);
    residency.requestResidency(1, 0, 1, origins);
    drainResidency(residency);
    const wgen::LocalClipmapUpdateIdentity oldIdentity =
        *residency.activeIdentity(0);
    const std::uint64_t previousRequests = residency.metrics().samplesRequested;

    origins[0].centerSample = {1, 0};
    residency.requestResidency(1, 0, 2, origins);
    wgen::tests::require(
        residency.hasCompleteActiveLevel(0) &&
            residency.activeIdentity(0) == oldIdentity &&
            residency.hasPendingTarget(0),
        "the previous complete cache should remain active during movement");
    wgen::tests::require(
        residency.desiredValidRegions(0) ==
            std::vector<wgen::LocalClipmapValidRegion>{{{-3, -4}, 8, 9}},
        "the overlapping target area should remain exactly valid");

    const auto requests = residency.takeGenerationRequestsForFrame();
    wgen::tests::require(
        generationSampleCount(requests) == 9 && requests.size() == 1,
        "a one-cell walk should generate exactly one exposed column");
    for (std::size_t index = 0; index < requests.front().coordinates.size(); ++index) {
        wgen::tests::require(
            requests.front().coordinates[index] ==
                wgen::LocalClipmapGridCoordinate{
                    5,
                    static_cast<std::int64_t>(index) - 4,
                },
            "the generated samples should be the newly exposed right column");
    }
    wgen::tests::require(
        residency.metrics().samplesRequested - previousRequests == 9,
        "normal movement should remain bounded to one column");
    residency.publishHeightUpdate(makeResult(requests.front()));
    const auto uploads = residency.takeGpuUploadBatchesForFrame();
    wgen::tests::require(
        uploads.size() == 1 && uploads.front().writes.size() == 9,
        "the exposed column should form one bounded upload batch");
    wgen::tests::require(
        uploads.front().writes.front().toroidalIndex ==
            wgen::localClipmapToroidalIndex({-4, -4}, 9),
        "a new column should reuse the toroidal slot of the retired column");
    wgen::tests::require(
        residency.activeGpuHeight(0, {-4, -4}).has_value(),
        "the old wrapped slot should remain readable until upload commit");
    residency.commitGpuUploadBatch(uploads.front());
    wgen::tests::require(
        residency.activeOrigin(0)->centerSample ==
                wgen::LocalClipmapGridCoordinate{1, 0} &&
            residency.activeGpuHeight(0, {5, -4}) ==
                syntheticHeight(*residency.activeIdentity(0), {5, -4}),
        "committing the strip should atomically promote the moved cache");
}

void testStaleResultsAndObsoleteWorkCoalescing() {
    const wgen::LocalClipmapConfig config = testClipmapConfig(1);
    wgen::LocalClipmapHeightResidency residency{
        config,
        {
            .generationBudgetSamplesPerFrame = 1'000,
            .uploadBudgetSamplesPerFrame = 1'000,
        },
    };
    auto origins = makeOrigins(config, 1);
    residency.requestResidency(1, 0, 1, origins);
    drainResidency(residency);
    const wgen::LocalClipmapUpdateIdentity fallback =
        *residency.activeIdentity(0);

    origins[0].centerSample = {1, 0};
    residency.requestResidency(1, 0, 2, origins);
    const auto staleRequest = residency.takeGenerationRequestsForFrame().front();
    origins[0].centerSample = {2, 0};
    residency.requestResidency(1, 0, 3, origins);
    wgen::tests::require(
        !residency.publishHeightUpdate(makeResult(staleRequest)) &&
            residency.activeIdentity(0) == fallback &&
            residency.desiredIdentity(0)->requestRevision == 3,
        "a stale generation result must not change active or desired residency");

    const auto currentRequests = residency.takeGenerationRequestsForFrame();
    for (const auto& request : currentRequests) {
        residency.publishHeightUpdate(makeResult(request));
    }
    const auto staleUpload = residency.takeGpuUploadBatchesForFrame().front();
    origins[0].centerSample = {3, 0};
    residency.requestResidency(1, 0, 4, origins);
    wgen::tests::require(
        !residency.commitGpuUploadBatch(staleUpload) &&
            residency.activeIdentity(0) == fallback &&
            residency.desiredIdentity(0)->requestRevision == 4,
        "a stale GPU acknowledgement must not corrupt the current frame");
    wgen::tests::require(
        residency.metrics().coalescedTargets >= 2 &&
            residency.metrics().staleGenerationResultsRejected == 1 &&
            residency.metrics().staleUploadBatchesRejected == 1,
        "coalescing and stale rejection should be observable in metrics");
    drainResidency(residency);
    wgen::tests::require(
        residency.activeIdentity(0)->requestRevision == 4,
        "coalesced current work should still converge");

    const wgen::LocalClipmapUpdateIdentity beforeDependencyChange =
        *residency.activeIdentity(0);
    residency.requestResidency(1, 1, 5, origins);
    wgen::tests::require(
        residency.pendingGenerationSampleCount() == 81 &&
            residency.desiredValidRegions(0).empty() &&
            residency.activeIdentity(0) == beforeDependencyChange,
        "a terrain dependency change should refill without discarding fallback residency");
}

void testTeleportAndReanchorRecovery() {
    const wgen::LocalClipmapConfig config = testClipmapConfig();
    constexpr std::size_t generationBudget = 13;
    constexpr std::size_t uploadBudget = 7;
    wgen::LocalClipmapHeightResidency residency{
        config,
        {
            .generationBudgetSamplesPerFrame = generationBudget,
            .uploadBudgetSamplesPerFrame = uploadBudget,
        },
    };
    auto origins = makeOrigins(config, 1);
    residency.requestResidency(2, 5, 1, origins);
    drainResidency(residency);
    const auto previousIdentities = std::array{
        *residency.activeIdentity(0),
        *residency.activeIdentity(1),
        *residency.activeIdentity(2),
    };

    origins = makeOrigins(config, 2, {1'000'000, -1'000'000});
    const std::uint64_t missesBefore = residency.metrics().cacheMisses;
    residency.requestResidency(2, 5, 2, origins);
    wgen::tests::require(
        residency.pendingGenerationSampleCount() == 3 * 81 &&
            residency.metrics().cacheMisses - missesBefore == 3 * 81,
        "a frame reanchor and teleport should invalidate every target sample");
    for (std::uint32_t level = 0; level < config.levelCount; ++level) {
        wgen::tests::require(
            residency.activeIdentity(level) == previousIdentities[level] &&
                residency.desiredValidRegions(level).empty(),
            "teleport refill should preserve old active data and expose no false validity");
    }

    const auto requests = residency.takeGenerationRequestsForFrame();
    wgen::tests::require(
        generationSampleCount(requests) <= generationBudget &&
            requests.front().identity.level == 0,
        "teleport recovery must obey the generation budget and priority");
    for (const auto& request : requests) {
        residency.publishHeightUpdate(makeResult(request));
    }
    const auto uploads = residency.takeGpuUploadBatchesForFrame();
    wgen::tests::require(
        uploadSampleCount(uploads) <= uploadBudget,
        "teleport recovery must obey the upload budget");
    for (const auto& upload : uploads) {
        residency.commitGpuUploadBatch(upload);
    }
    wgen::tests::require(
        !residency.desiredValidRegions(0).empty() &&
            residency.hasPendingTarget(0),
        "partial teleport recovery should report exact valid data without promotion");

    drainResidency(residency);
    for (std::uint32_t level = 0; level < config.levelCount; ++level) {
        wgen::tests::require(
            residency.activeIdentity(level)->localFrameRevision == 2 &&
                residency.activeOrigin(level)->centerSample ==
                    wgen::LocalClipmapGridCoordinate{1'000'000, -1'000'000} &&
                !residency.hasPendingTarget(level),
            "teleport recovery should converge every level to the new frame");
    }
    wgen::tests::require(
        residency.metrics().completedUpdates >= 2 * config.levelCount &&
            residency.metrics().lastUpdateLatencyMilliseconds >= 0.0 &&
            residency.metrics().cacheHitRate() >= 0.0 &&
            residency.metrics().cacheHitRate() <= 1.0,
        "residency should expose bounded update latency and cache metrics");
}

void testSnapshotHeightGeneration() {
    const wgen::LocalClipmapConfig config = testClipmapConfig(1);
    wgen::LocalClipmapHeightResidency residency{
        config,
        {
            .generationBudgetSamplesPerFrame = 5,
            .uploadBudgetSamplesPerFrame = 5,
        },
    };
    residency.requestResidency(1, 0, 1, makeOrigins(config, 9));
    const wgen::LocalClipmapHeightUpdateRequest request =
        residency.takeGenerationRequestsForFrame().front();
    const wgen::TerrainFieldSnapshot field =
        wgen::buildTerrainFieldSnapshot({}, 1, 100.0F);
    const wgen::LocalPlanetFrame frame = wgen::makeLocalPlanetFrame(
        glm::dvec3{0.0, 0.0, 1.0},
        field->radius(),
        9);
    const wgen::LocalClipmapHeightUpdateResult result =
        wgen::generateLocalClipmapHeightUpdate(
            request,
            frame,
            field,
            config);
    wgen::tests::require(
        result.writes.size() == request.coordinates.size() &&
            std::all_of(result.writes.begin(), result.writes.end(), [](const auto& write) {
                return write.heightMeters == 0.0F;
            }) &&
            result.generationLatencyMilliseconds >= 0.0,
        "height work should sample one immutable terrain snapshot on the CPU");
    wgen::tests::require(
        residency.publishHeightUpdate(result),
        "snapshot-generated height work should publish normally");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest(
            "identity, priority, and toroidal addressing",
            testIdentityPriorityAndToroidalAddressing);
        wgen::tests::runTest(
            "strict budgets, priority, and partial validity",
            testStrictBudgetsPriorityAndPartialValidity);
        wgen::tests::runTest(
            "bounded exposed-column movement",
            testSmallMovementUpdatesOnlyExposedColumn);
        wgen::tests::runTest(
            "stale rejection and coalescing",
            testStaleResultsAndObsoleteWorkCoalescing);
        wgen::tests::runTest(
            "teleport and reanchor recovery",
            testTeleportAndReanchorRecovery);
        wgen::tests::runTest(
            "immutable snapshot height generation",
            testSnapshotHeightGeneration);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
