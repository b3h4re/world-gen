#pragma once

#include "terrain/planet/local_clipmap_sampling.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace wgen {

inline constexpr std::size_t DEFAULT_LOCAL_CLIPMAP_GENERATION_BUDGET_SAMPLES = 4096;
inline constexpr std::size_t DEFAULT_LOCAL_CLIPMAP_UPLOAD_BUDGET_SAMPLES = 4096;

struct LocalClipmapResidencyConfig {
    std::size_t generationBudgetSamplesPerFrame{
        DEFAULT_LOCAL_CLIPMAP_GENERATION_BUDGET_SAMPLES};
    std::size_t uploadBudgetSamplesPerFrame{
        DEFAULT_LOCAL_CLIPMAP_UPLOAD_BUDGET_SAMPLES};
};

struct LocalClipmapUpdateIdentity {
    std::uint64_t terrainEpoch{};
    std::uint64_t terrainDependencyRevision{};
    std::uint64_t localFrameRevision{};
    std::uint32_t level{};
    LocalClipmapGridCoordinate snappedGridOrigin{};
    std::uint64_t requestRevision{};

    bool operator==(const LocalClipmapUpdateIdentity&) const = default;
};

struct LocalClipmapHeightWrite {
    LocalClipmapGridCoordinate coordinate{};
    float heightMeters{};

    bool operator==(const LocalClipmapHeightWrite&) const = default;
};

struct LocalClipmapHeightUpdateRequest {
    LocalClipmapUpdateIdentity identity{};
    LocalClipmapCacheOrigin cacheOrigin{};
    std::uint64_t workId{};
    std::vector<LocalClipmapGridCoordinate> coordinates{};
};

struct LocalClipmapHeightUpdateResult {
    LocalClipmapUpdateIdentity identity{};
    std::uint64_t workId{};
    std::vector<LocalClipmapHeightWrite> writes{};
    double generationLatencyMilliseconds{};
};

struct LocalClipmapGpuHeightWrite {
    LocalClipmapGridCoordinate coordinate{};
    std::uint32_t toroidalIndex{};
    float heightMeters{};

    bool operator==(const LocalClipmapGpuHeightWrite&) const = default;
};

// Step 11 applies this batch to its renderer-owned height buffer, then calls
// commitGpuUploadBatch. Until that acknowledgement, the samples are invalid
// for hybrid ownership.
struct LocalClipmapGpuUploadBatch {
    LocalClipmapUpdateIdentity identity{};
    std::uint64_t uploadId{};
    std::vector<LocalClipmapGpuHeightWrite> writes{};
};

struct LocalClipmapValidRegion {
    LocalClipmapGridCoordinate minimum{};
    std::uint32_t widthSamples{};
    std::uint32_t heightSamples{};

    bool operator==(const LocalClipmapValidRegion&) const = default;
};

struct LocalClipmapResidencyMetrics {
    std::uint64_t samplesRequested{};
    std::uint64_t samplesGenerated{};
    std::uint64_t samplesUploaded{};
    std::uint64_t bytesUploaded{};
    std::uint64_t cacheHits{};
    std::uint64_t cacheMisses{};
    std::uint64_t staleGenerationResultsRejected{};
    std::uint64_t staleUploadBatchesRejected{};
    std::uint64_t coalescedTargets{};
    std::uint64_t completedUpdates{};
    double totalGenerationLatencyMilliseconds{};
    double lastUpdateLatencyMilliseconds{};
    double maximumUpdateLatencyMilliseconds{};

    double cacheHitRate() const noexcept;
};

void validateLocalClipmapResidencyConfig(
    const LocalClipmapResidencyConfig& config);

void validateLocalClipmapUpdateIdentity(
    const LocalClipmapUpdateIdentity& identity,
    const LocalClipmapConfig& clipmapConfig);

std::uint32_t localClipmapToroidalIndex(
    const LocalClipmapGridCoordinate& coordinate,
    std::uint32_t samplesPerAxis);

// Lower values are scheduled first: fine center, outer handoff ring, then
// intermediate rings from fine to coarse.
std::size_t localClipmapResidencyPriority(
    std::uint32_t level,
    std::uint32_t levelCount);

LocalClipmapHeightUpdateResult generateLocalClipmapHeightUpdate(
    const LocalClipmapHeightUpdateRequest& request,
    const LocalPlanetFrame& frame,
    const TerrainFieldSnapshot& terrainField,
    const LocalClipmapConfig& clipmapConfig);

class LocalClipmapHeightResidency {
public:
    explicit LocalClipmapHeightResidency(
        LocalClipmapConfig clipmapConfig,
        LocalClipmapResidencyConfig residencyConfig = {});

    // All configured levels are requested together. A strictly newer request
    // coalesces obsolete work while retaining reusable samples.
    void requestResidency(
        std::uint64_t terrainEpoch,
        std::uint64_t terrainDependencyRevision,
        std::uint64_t requestRevision,
        std::span<const LocalClipmapCacheOrigin> origins);

    std::vector<LocalClipmapHeightUpdateRequest>
        takeGenerationRequestsForFrame();
    bool publishHeightUpdate(const LocalClipmapHeightUpdateResult& result);
    void cancelHeightUpdate(const LocalClipmapHeightUpdateRequest& request);

    std::vector<LocalClipmapGpuUploadBatch> takeGpuUploadBatchesForFrame();
    bool commitGpuUploadBatch(const LocalClipmapGpuUploadBatch& batch);
    void cancelGpuUploadBatch(const LocalClipmapGpuUploadBatch& batch);

    bool hasCompleteActiveLevel(std::uint32_t level) const;
    bool hasPendingTarget(std::uint32_t level) const;
    bool isDesiredSampleValid(
        std::uint32_t level,
        const LocalClipmapGridCoordinate& coordinate) const;

    std::optional<LocalClipmapUpdateIdentity> activeIdentity(
        std::uint32_t level) const;
    std::optional<LocalClipmapUpdateIdentity> desiredIdentity(
        std::uint32_t level) const;
    std::optional<LocalClipmapCacheOrigin> activeOrigin(
        std::uint32_t level) const;
    std::optional<LocalClipmapCacheOrigin> desiredOrigin(
        std::uint32_t level) const;

    std::vector<LocalClipmapValidRegion> desiredValidRegions(
        std::uint32_t level) const;
    std::vector<LocalClipmapValidRegion> activeValidRegions(
        std::uint32_t level) const;

    std::optional<float> desiredCpuHeight(
        std::uint32_t level,
        const LocalClipmapGridCoordinate& coordinate) const;
    std::optional<float> desiredGpuHeight(
        std::uint32_t level,
        const LocalClipmapGridCoordinate& coordinate) const;
    std::optional<float> activeGpuHeight(
        std::uint32_t level,
        const LocalClipmapGridCoordinate& coordinate) const;

    std::size_t pendingGenerationSampleCount() const;
    std::size_t pendingUploadSampleCount() const;

    const LocalClipmapConfig& clipmapConfig() const { return clipmapConfig_; }
    const LocalClipmapResidencyConfig& residencyConfig() const {
        return residencyConfig_;
    }
    const LocalClipmapResidencyMetrics& metrics() const { return metrics_; }

private:
    enum class CellState : std::uint8_t {
        Missing,
        GenerationPending,
        CpuReady,
        UploadPending,
        Resident,
    };

    struct CacheCell {
        LocalClipmapGridCoordinate coordinate{};
        float cpuHeightMeters{};
        float gpuHeightMeters{};
        CellState state{CellState::Missing};
        std::uint64_t pendingWorkId{};
        std::uint64_t pendingUploadId{};
    };

    struct CacheBuffer {
        LocalClipmapUpdateIdentity identity{};
        LocalClipmapCacheOrigin origin{};
        LocalClipmapGridCoordinate windowMinimum{};
        std::vector<CacheCell> cells{};
        std::chrono::steady_clock::time_point requestedAt{};
    };

    struct LevelState {
        std::optional<CacheBuffer> active{};
        std::optional<CacheBuffer> staging{};
    };

    struct GenerationWorkRecord {
        LocalClipmapUpdateIdentity identity{};
        std::uint32_t level{};
        std::vector<LocalClipmapGridCoordinate> coordinates{};
    };

    struct UploadWorkRecord {
        LocalClipmapUpdateIdentity identity{};
        std::uint32_t level{};
        std::vector<LocalClipmapGpuHeightWrite> writes{};
    };

    const CacheBuffer* desiredBuffer(std::uint32_t level) const;
    CacheBuffer* desiredBuffer(std::uint32_t level);
    const CacheCell* findCell(
        const CacheBuffer& buffer,
        const LocalClipmapGridCoordinate& coordinate) const;
    CacheCell* findCell(
        CacheBuffer& buffer,
        const LocalClipmapGridCoordinate& coordinate);
    CacheBuffer makeTargetBuffer(
        const LocalClipmapUpdateIdentity& identity,
        const LocalClipmapCacheOrigin& origin) const;
    bool contentCompatible(
        const CacheBuffer& source,
        const CacheBuffer& target) const;
    std::size_t reuseSamples(
        const CacheBuffer& source,
        CacheBuffer& target) const;
    std::vector<LocalClipmapValidRegion> validRegions(
        const CacheBuffer& buffer) const;
    void validateLevel(std::uint32_t level) const;
    void promoteIfComplete(std::uint32_t level);
    void eraseObsoleteWork(std::uint32_t level);
    std::vector<std::uint32_t> prioritizedLevels() const;

    LocalClipmapConfig clipmapConfig_{};
    LocalClipmapResidencyConfig residencyConfig_{};
    std::vector<LevelState> levels_{};
    std::unordered_map<std::uint64_t, GenerationWorkRecord> generationWork_{};
    std::unordered_map<std::uint64_t, UploadWorkRecord> uploadWork_{};
    std::uint64_t latestRequestRevision_{};
    std::uint64_t nextWorkId_{1};
    std::uint64_t nextUploadId_{1};
    LocalClipmapResidencyMetrics metrics_{};
};

} // namespace wgen
