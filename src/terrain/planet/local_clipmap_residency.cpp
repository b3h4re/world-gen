#include "terrain/planet/local_clipmap_residency.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace wgen {
namespace {

bool isFinite(const glm::dvec2& value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

std::int64_t checkedAdd(std::int64_t value, std::int64_t offset) {
    if ((offset > 0 && value > std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 && value < std::numeric_limits<std::int64_t>::min() - offset)) {
        throw std::overflow_error{"local clipmap residency coordinate overflow"};
    }
    return value + offset;
}

bool coordinateInWindow(
        const LocalClipmapGridCoordinate& coordinate,
        const LocalClipmapGridCoordinate& minimum,
        std::uint32_t samplesPerAxis) {
    const auto size = static_cast<std::int64_t>(samplesPerAxis);
    const std::int64_t maximumX = checkedAdd(minimum.x, size);
    const std::int64_t maximumY = checkedAdd(minimum.y, size);
    return coordinate.x >= minimum.x && coordinate.x < maximumX &&
        coordinate.y >= minimum.y && coordinate.y < maximumY;
}

bool sameOrigin(
        const LocalClipmapCacheOrigin& lhs,
        const LocalClipmapCacheOrigin& rhs) {
    return lhs.localFrameAnchorMeters == rhs.localFrameAnchorMeters &&
        lhs.frameRevision == rhs.frameRevision && lhs.level == rhs.level &&
        lhs.centerSample == rhs.centerSample;
}

std::uint64_t nextNonZero(std::uint64_t& value) {
    const std::uint64_t result = value++;
    if (result == 0 || value == 0) {
        throw std::overflow_error{"local clipmap work identifier overflow"};
    }
    return result;
}

} // namespace

double LocalClipmapResidencyMetrics::cacheHitRate() const noexcept {
    const std::uint64_t total = cacheHits + cacheMisses;
    return total == 0
        ? 1.0
        : static_cast<double>(cacheHits) / static_cast<double>(total);
}

void validateLocalClipmapResidencyConfig(
        const LocalClipmapResidencyConfig& config) {
    if (config.generationBudgetSamplesPerFrame == 0 ||
            config.uploadBudgetSamplesPerFrame == 0) {
        throw std::invalid_argument{
            "local clipmap generation and upload budgets must be positive"};
    }
}

void validateLocalClipmapUpdateIdentity(
        const LocalClipmapUpdateIdentity& identity,
        const LocalClipmapConfig& clipmapConfig) {
    validateLocalClipmapConfig(clipmapConfig);
    if (identity.terrainEpoch == 0 || identity.localFrameRevision == 0 ||
            identity.requestRevision == 0) {
        throw std::invalid_argument{
            "local clipmap update identity versions must be initialized"};
    }
    if (identity.level >= clipmapConfig.levelCount) {
        throw std::out_of_range{
            "local clipmap update identity level is outside the configured range"};
    }
}

std::uint32_t localClipmapToroidalIndex(
        const LocalClipmapGridCoordinate& coordinate,
        std::uint32_t samplesPerAxis) {
    if (samplesPerAxis == 0) {
        throw std::invalid_argument{
            "local clipmap toroidal cache size must be positive"};
    }
    const std::uint64_t sampleCount =
        static_cast<std::uint64_t>(samplesPerAxis) * samplesPerAxis;
    if (sampleCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error{
            "local clipmap toroidal cache exceeds 32-bit indices"};
    }
    const auto size = static_cast<std::int64_t>(samplesPerAxis);
    const auto wrap = [size](std::int64_t value) {
        std::int64_t result = value % size;
        if (result < 0) {
            result += size;
        }
        return static_cast<std::uint32_t>(result);
    };
    return wrap(coordinate.y) * samplesPerAxis + wrap(coordinate.x);
}

std::size_t localClipmapResidencyPriority(
        std::uint32_t level,
        std::uint32_t levelCount) {
    if (levelCount == 0 || level >= levelCount) {
        throw std::out_of_range{
            "local clipmap residency priority level is outside the active range"};
    }
    if (level == 0) {
        return 0;
    }
    if (levelCount > 1 && level + 1 == levelCount) {
        return 1;
    }
    return static_cast<std::size_t>(level) + 1;
}

LocalClipmapHeightUpdateResult generateLocalClipmapHeightUpdate(
        const LocalClipmapHeightUpdateRequest& request,
        const LocalPlanetFrame& frame,
        const TerrainFieldSnapshot& terrainField,
        const LocalClipmapConfig& clipmapConfig) {
    validateLocalClipmapUpdateIdentity(request.identity, clipmapConfig);
    if (request.workId == 0 || request.coordinates.empty()) {
        throw std::invalid_argument{
            "local clipmap height request must contain initialized work"};
    }
    if (request.cacheOrigin.frameRevision !=
                request.identity.localFrameRevision ||
            request.cacheOrigin.level != request.identity.level ||
            request.cacheOrigin.centerSample !=
                request.identity.snappedGridOrigin) {
        throw std::invalid_argument{
            "local clipmap height request origin disagrees with its identity"};
    }
    const LocalClipmapGridCoordinate minimum =
        localClipmapWindowMinimum(clipmapConfig, request.cacheOrigin);
    std::set<std::pair<std::int64_t, std::int64_t>> uniqueCoordinates;
    for (const LocalClipmapGridCoordinate& coordinate : request.coordinates) {
        if (!coordinateInWindow(
                coordinate,
                minimum,
                clipmapConfig.samplesPerAxis) ||
                !uniqueCoordinates.emplace(coordinate.x, coordinate.y).second) {
            throw std::invalid_argument{
                "local clipmap height request coordinates are invalid"};
        }
    }

    const auto startedAt = std::chrono::steady_clock::now();
    const std::vector<LocalClipmapSurfaceSample> sampled =
        sampleLocalClipmapPoints(
            frame,
            terrainField,
            clipmapConfig,
            request.cacheOrigin,
            request.coordinates,
            frame.globalAnchorPositionMeters);
    const auto finishedAt = std::chrono::steady_clock::now();

    LocalClipmapHeightUpdateResult result{
        .identity = request.identity,
        .workId = request.workId,
        .generationLatencyMilliseconds =
            std::chrono::duration<double, std::milli>(
                finishedAt - startedAt).count(),
    };
    result.writes.reserve(sampled.size());
    for (const LocalClipmapSurfaceSample& sample : sampled) {
        result.writes.push_back({
            sample.gridCoordinate,
            sample.heightMeters,
        });
    }
    return result;
}

LocalClipmapHeightResidency::LocalClipmapHeightResidency(
        LocalClipmapConfig clipmapConfig,
        LocalClipmapResidencyConfig residencyConfig)
    : clipmapConfig_{std::move(clipmapConfig)},
      residencyConfig_{std::move(residencyConfig)} {
    validateLocalClipmapConfig(clipmapConfig_);
    validateLocalClipmapResidencyConfig(residencyConfig_);
    levels_.resize(clipmapConfig_.levelCount);
}

void LocalClipmapHeightResidency::requestResidency(
        std::uint64_t terrainEpoch,
        std::uint64_t terrainDependencyRevision,
        std::uint64_t requestRevision,
        std::span<const LocalClipmapCacheOrigin> origins) {
    if (terrainEpoch == 0 || requestRevision == 0) {
        throw std::invalid_argument{
            "local clipmap residency versions must be initialized"};
    }
    if (origins.size() != clipmapConfig_.levelCount) {
        throw std::invalid_argument{
            "local clipmap residency requires one origin per level"};
    }

    std::vector<LocalClipmapUpdateIdentity> identities;
    identities.reserve(origins.size());
    for (std::uint32_t level = 0; level < origins.size(); ++level) {
        const LocalClipmapCacheOrigin& origin = origins[level];
        if (origin.level != level || origin.frameRevision == 0 ||
                !isFinite(origin.localFrameAnchorMeters) ||
                (level > 0 &&
                    (origin.frameRevision != origins.front().frameRevision ||
                        origin.localFrameAnchorMeters !=
                            origins.front().localFrameAnchorMeters))) {
            throw std::invalid_argument{
                "local clipmap residency origins must share one ordered local frame"};
        }
        LocalClipmapUpdateIdentity identity{
            .terrainEpoch = terrainEpoch,
            .terrainDependencyRevision = terrainDependencyRevision,
            .localFrameRevision = origin.frameRevision,
            .level = level,
            .snappedGridOrigin = origin.centerSample,
            .requestRevision = requestRevision,
        };
        validateLocalClipmapUpdateIdentity(identity, clipmapConfig_);
        identities.push_back(identity);
    }

    if (requestRevision < latestRequestRevision_) {
        throw std::invalid_argument{
            "local clipmap residency request revision is stale"};
    }
    if (requestRevision == latestRequestRevision_) {
        bool unchanged = true;
        for (std::uint32_t level = 0; level < levels_.size(); ++level) {
            const CacheBuffer* current = desiredBuffer(level);
            unchanged = unchanged && current != nullptr &&
                current->identity == identities[level] &&
                sameOrigin(current->origin, origins[level]);
        }
        if (unchanged) {
            return;
        }
        throw std::invalid_argument{
            "one request revision cannot identify different clipmap targets"};
    }
    latestRequestRevision_ = requestRevision;

    for (std::uint32_t level = 0; level < levels_.size(); ++level) {
        LevelState& state = levels_[level];
        std::optional<CacheBuffer> obsoleteStaging =
            std::move(state.staging);
        if (obsoleteStaging) {
            ++metrics_.coalescedTargets;
        }
        eraseObsoleteWork(level);

        CacheBuffer target = makeTargetBuffer(identities[level], origins[level]);
        std::size_t reused = 0;
        if (state.active && contentCompatible(*state.active, target)) {
            reused += reuseSamples(*state.active, target);
        }
        if (obsoleteStaging && contentCompatible(*obsoleteStaging, target)) {
            reused += reuseSamples(*obsoleteStaging, target);
        }

        const std::size_t sampleCount = target.cells.size();
        metrics_.cacheHits += reused;
        metrics_.cacheMisses += sampleCount - reused;
        state.staging = std::move(target);
        promoteIfComplete(level);
    }
}

std::vector<LocalClipmapHeightUpdateRequest>
LocalClipmapHeightResidency::takeGenerationRequestsForFrame() {
    std::size_t remaining =
        residencyConfig_.generationBudgetSamplesPerFrame;
    std::vector<LocalClipmapHeightUpdateRequest> requests;
    const auto size = static_cast<std::int64_t>(clipmapConfig_.samplesPerAxis);

    for (const std::uint32_t level : prioritizedLevels()) {
        if (remaining == 0 || !levels_[level].staging) {
            continue;
        }
        CacheBuffer& target = *levels_[level].staging;
        std::vector<LocalClipmapGridCoordinate> coordinates;
        coordinates.reserve(std::min(remaining, target.cells.size()));
        for (std::int64_t yOffset = 0;
             yOffset < size && coordinates.size() < remaining;
             ++yOffset) {
            for (std::int64_t xOffset = 0;
                 xOffset < size && coordinates.size() < remaining;
                 ++xOffset) {
                const LocalClipmapGridCoordinate coordinate{
                    checkedAdd(target.windowMinimum.x, xOffset),
                    checkedAdd(target.windowMinimum.y, yOffset),
                };
                CacheCell* cell = findCell(target, coordinate);
                if (cell != nullptr && cell->state == CellState::Missing) {
                    coordinates.push_back(coordinate);
                }
            }
        }
        if (coordinates.empty()) {
            continue;
        }

        const std::uint64_t workId = nextNonZero(nextWorkId_);
        for (const LocalClipmapGridCoordinate& coordinate : coordinates) {
            CacheCell* cell = findCell(target, coordinate);
            cell->state = CellState::GenerationPending;
            cell->pendingWorkId = workId;
        }
        generationWork_.emplace(workId, GenerationWorkRecord{
            target.identity,
            level,
            coordinates,
        });
        metrics_.samplesRequested += coordinates.size();
        remaining -= coordinates.size();
        requests.push_back({
            target.identity,
            target.origin,
            workId,
            std::move(coordinates),
        });
    }
    return requests;
}

bool LocalClipmapHeightResidency::publishHeightUpdate(
        const LocalClipmapHeightUpdateResult& result) {
    validateLocalClipmapUpdateIdentity(result.identity, clipmapConfig_);
    if (result.workId == 0 || result.writes.empty() ||
            !std::isfinite(result.generationLatencyMilliseconds) ||
            result.generationLatencyMilliseconds < 0.0) {
        throw std::invalid_argument{
            "local clipmap height result is invalid"};
    }
    std::set<std::pair<std::int64_t, std::int64_t>> uniqueCoordinates;
    for (const LocalClipmapHeightWrite& write : result.writes) {
        if (!std::isfinite(write.heightMeters) ||
                !uniqueCoordinates.emplace(
                    write.coordinate.x,
                    write.coordinate.y).second) {
            throw std::invalid_argument{
                "local clipmap height result contains invalid writes"};
        }
    }
    metrics_.samplesGenerated += result.writes.size();
    metrics_.totalGenerationLatencyMilliseconds +=
        result.generationLatencyMilliseconds;

    validateLevel(result.identity.level);
    LevelState& state = levels_[result.identity.level];
    if (!state.staging || state.staging->identity != result.identity) {
        ++metrics_.staleGenerationResultsRejected;
        return false;
    }
    const auto record = generationWork_.find(result.workId);
    if (record == generationWork_.end() ||
            record->second.identity != result.identity ||
            record->second.coordinates.size() != result.writes.size()) {
        throw std::invalid_argument{
            "local clipmap height result does not match pending work"};
    }
    for (std::size_t index = 0; index < result.writes.size(); ++index) {
        if (result.writes[index].coordinate !=
                record->second.coordinates[index]) {
            throw std::invalid_argument{
                "local clipmap height result changed request ordering"};
        }
        CacheCell* cell = findCell(
            *state.staging,
            result.writes[index].coordinate);
        if (cell == nullptr ||
                cell->state != CellState::GenerationPending ||
                cell->pendingWorkId != result.workId) {
            throw std::invalid_argument{
                "local clipmap height result targets non-pending samples"};
        }
    }
    for (const LocalClipmapHeightWrite& write : result.writes) {
        CacheCell* cell = findCell(*state.staging, write.coordinate);
        cell->cpuHeightMeters = write.heightMeters;
        cell->state = CellState::CpuReady;
        cell->pendingWorkId = 0;
    }
    generationWork_.erase(record);
    return true;
}

void LocalClipmapHeightResidency::cancelHeightUpdate(
        const LocalClipmapHeightUpdateRequest& request) {
    const auto record = generationWork_.find(request.workId);
    if (record == generationWork_.end()) {
        return;
    }
    const std::uint32_t level = record->second.level;
    if (levels_[level].staging &&
            levels_[level].staging->identity == record->second.identity) {
        for (const LocalClipmapGridCoordinate& coordinate :
                record->second.coordinates) {
            CacheCell* cell = findCell(*levels_[level].staging, coordinate);
            if (cell && cell->state == CellState::GenerationPending &&
                    cell->pendingWorkId == request.workId) {
                cell->state = CellState::Missing;
                cell->pendingWorkId = 0;
            }
        }
    }
    generationWork_.erase(record);
}

std::vector<LocalClipmapGpuUploadBatch>
LocalClipmapHeightResidency::takeGpuUploadBatchesForFrame() {
    std::size_t remaining = residencyConfig_.uploadBudgetSamplesPerFrame;
    std::vector<LocalClipmapGpuUploadBatch> batches;
    const auto size = static_cast<std::int64_t>(clipmapConfig_.samplesPerAxis);

    for (const std::uint32_t level : prioritizedLevels()) {
        if (remaining == 0 || !levels_[level].staging) {
            continue;
        }
        CacheBuffer& target = *levels_[level].staging;
        std::vector<LocalClipmapGpuHeightWrite> writes;
        writes.reserve(std::min(remaining, target.cells.size()));
        for (std::int64_t yOffset = 0;
             yOffset < size && writes.size() < remaining;
             ++yOffset) {
            for (std::int64_t xOffset = 0;
                 xOffset < size && writes.size() < remaining;
                 ++xOffset) {
                const LocalClipmapGridCoordinate coordinate{
                    checkedAdd(target.windowMinimum.x, xOffset),
                    checkedAdd(target.windowMinimum.y, yOffset),
                };
                CacheCell* cell = findCell(target, coordinate);
                if (cell != nullptr && cell->state == CellState::CpuReady) {
                    writes.push_back({
                        coordinate,
                        localClipmapToroidalIndex(
                            coordinate,
                            clipmapConfig_.samplesPerAxis),
                        cell->cpuHeightMeters,
                    });
                }
            }
        }
        if (writes.empty()) {
            continue;
        }

        const std::uint64_t uploadId = nextNonZero(nextUploadId_);
        for (const LocalClipmapGpuHeightWrite& write : writes) {
            CacheCell* cell = findCell(target, write.coordinate);
            cell->state = CellState::UploadPending;
            cell->pendingUploadId = uploadId;
        }
        uploadWork_.emplace(uploadId, UploadWorkRecord{
            target.identity,
            level,
            writes,
        });
        remaining -= writes.size();
        batches.push_back({
            target.identity,
            uploadId,
            std::move(writes),
        });
    }
    return batches;
}

bool LocalClipmapHeightResidency::commitGpuUploadBatch(
        const LocalClipmapGpuUploadBatch& batch) {
    validateLocalClipmapUpdateIdentity(batch.identity, clipmapConfig_);
    if (batch.uploadId == 0 || batch.writes.empty()) {
        throw std::invalid_argument{
            "local clipmap GPU upload batch is empty or uninitialized"};
    }
    validateLevel(batch.identity.level);
    LevelState& state = levels_[batch.identity.level];
    if (!state.staging || state.staging->identity != batch.identity) {
        ++metrics_.staleUploadBatchesRejected;
        return false;
    }
    const auto record = uploadWork_.find(batch.uploadId);
    if (record == uploadWork_.end() ||
            record->second.identity != batch.identity ||
            record->second.writes != batch.writes) {
        throw std::invalid_argument{
            "local clipmap GPU upload does not match pending work"};
    }
    for (const LocalClipmapGpuHeightWrite& write : batch.writes) {
        if (!std::isfinite(write.heightMeters) ||
                write.toroidalIndex != localClipmapToroidalIndex(
                    write.coordinate,
                    clipmapConfig_.samplesPerAxis)) {
            throw std::invalid_argument{
                "local clipmap GPU upload write is invalid"};
        }
        CacheCell* cell = findCell(*state.staging, write.coordinate);
        if (cell == nullptr || cell->state != CellState::UploadPending ||
                cell->pendingUploadId != batch.uploadId ||
                cell->cpuHeightMeters != write.heightMeters) {
            throw std::invalid_argument{
                "local clipmap GPU upload targets non-pending samples"};
        }
    }
    for (const LocalClipmapGpuHeightWrite& write : batch.writes) {
        CacheCell* cell = findCell(*state.staging, write.coordinate);
        cell->gpuHeightMeters = write.heightMeters;
        cell->state = CellState::Resident;
        cell->pendingUploadId = 0;
    }
    metrics_.samplesUploaded += batch.writes.size();
    metrics_.bytesUploaded += batch.writes.size() * sizeof(float);
    uploadWork_.erase(record);
    promoteIfComplete(batch.identity.level);
    return true;
}

void LocalClipmapHeightResidency::cancelGpuUploadBatch(
        const LocalClipmapGpuUploadBatch& batch) {
    const auto record = uploadWork_.find(batch.uploadId);
    if (record == uploadWork_.end()) {
        return;
    }
    const std::uint32_t level = record->second.level;
    if (levels_[level].staging &&
            levels_[level].staging->identity == record->second.identity) {
        for (const LocalClipmapGpuHeightWrite& write : record->second.writes) {
            CacheCell* cell = findCell(*levels_[level].staging, write.coordinate);
            if (cell && cell->state == CellState::UploadPending &&
                    cell->pendingUploadId == batch.uploadId) {
                cell->state = CellState::CpuReady;
                cell->pendingUploadId = 0;
            }
        }
    }
    uploadWork_.erase(record);
}

bool LocalClipmapHeightResidency::hasCompleteActiveLevel(
        std::uint32_t level) const {
    validateLevel(level);
    return levels_[level].active.has_value();
}

bool LocalClipmapHeightResidency::hasPendingTarget(
        std::uint32_t level) const {
    validateLevel(level);
    return levels_[level].staging.has_value();
}

bool LocalClipmapHeightResidency::isDesiredSampleValid(
        std::uint32_t level,
        const LocalClipmapGridCoordinate& coordinate) const {
    validateLevel(level);
    const CacheBuffer* buffer = desiredBuffer(level);
    const CacheCell* cell = buffer ? findCell(*buffer, coordinate) : nullptr;
    return cell != nullptr && cell->state == CellState::Resident;
}

std::optional<LocalClipmapUpdateIdentity>
LocalClipmapHeightResidency::activeIdentity(std::uint32_t level) const {
    validateLevel(level);
    if (!levels_[level].active) {
        return std::nullopt;
    }
    return levels_[level].active->identity;
}

std::optional<LocalClipmapUpdateIdentity>
LocalClipmapHeightResidency::desiredIdentity(std::uint32_t level) const {
    validateLevel(level);
    const CacheBuffer* buffer = desiredBuffer(level);
    return buffer ? std::optional{buffer->identity} : std::nullopt;
}

std::optional<LocalClipmapCacheOrigin>
LocalClipmapHeightResidency::activeOrigin(std::uint32_t level) const {
    validateLevel(level);
    if (!levels_[level].active) {
        return std::nullopt;
    }
    return levels_[level].active->origin;
}

std::optional<LocalClipmapCacheOrigin>
LocalClipmapHeightResidency::desiredOrigin(std::uint32_t level) const {
    validateLevel(level);
    const CacheBuffer* buffer = desiredBuffer(level);
    return buffer ? std::optional{buffer->origin} : std::nullopt;
}

std::vector<LocalClipmapValidRegion>
LocalClipmapHeightResidency::desiredValidRegions(std::uint32_t level) const {
    validateLevel(level);
    const CacheBuffer* buffer = desiredBuffer(level);
    return buffer ? validRegions(*buffer) : std::vector<LocalClipmapValidRegion>{};
}

std::vector<LocalClipmapValidRegion>
LocalClipmapHeightResidency::activeValidRegions(std::uint32_t level) const {
    validateLevel(level);
    return levels_[level].active
        ? validRegions(*levels_[level].active)
        : std::vector<LocalClipmapValidRegion>{};
}

std::optional<float> LocalClipmapHeightResidency::desiredCpuHeight(
        std::uint32_t level,
        const LocalClipmapGridCoordinate& coordinate) const {
    validateLevel(level);
    const CacheBuffer* buffer = desiredBuffer(level);
    const CacheCell* cell = buffer ? findCell(*buffer, coordinate) : nullptr;
    if (cell == nullptr || cell->state == CellState::Missing ||
            cell->state == CellState::GenerationPending) {
        return std::nullopt;
    }
    return cell->cpuHeightMeters;
}

std::optional<float> LocalClipmapHeightResidency::desiredGpuHeight(
        std::uint32_t level,
        const LocalClipmapGridCoordinate& coordinate) const {
    validateLevel(level);
    const CacheBuffer* buffer = desiredBuffer(level);
    const CacheCell* cell = buffer ? findCell(*buffer, coordinate) : nullptr;
    if (cell == nullptr || cell->state != CellState::Resident) {
        return std::nullopt;
    }
    return cell->gpuHeightMeters;
}

std::optional<float> LocalClipmapHeightResidency::activeGpuHeight(
        std::uint32_t level,
        const LocalClipmapGridCoordinate& coordinate) const {
    validateLevel(level);
    if (!levels_[level].active) {
        return std::nullopt;
    }
    const CacheCell* cell = findCell(*levels_[level].active, coordinate);
    if (cell == nullptr || cell->state != CellState::Resident) {
        return std::nullopt;
    }
    return cell->gpuHeightMeters;
}

std::size_t LocalClipmapHeightResidency::pendingGenerationSampleCount() const {
    std::size_t result = 0;
    for (const LevelState& level : levels_) {
        if (!level.staging) {
            continue;
        }
        result += std::count_if(
            level.staging->cells.begin(),
            level.staging->cells.end(),
            [](const CacheCell& cell) {
                return cell.state == CellState::Missing ||
                    cell.state == CellState::GenerationPending;
            });
    }
    return result;
}

std::size_t LocalClipmapHeightResidency::pendingUploadSampleCount() const {
    std::size_t result = 0;
    for (const LevelState& level : levels_) {
        if (!level.staging) {
            continue;
        }
        result += std::count_if(
            level.staging->cells.begin(),
            level.staging->cells.end(),
            [](const CacheCell& cell) {
                return cell.state == CellState::CpuReady ||
                    cell.state == CellState::UploadPending;
            });
    }
    return result;
}

const LocalClipmapHeightResidency::CacheBuffer*
LocalClipmapHeightResidency::desiredBuffer(std::uint32_t level) const {
    validateLevel(level);
    if (levels_[level].staging) {
        return &*levels_[level].staging;
    }
    return levels_[level].active ? &*levels_[level].active : nullptr;
}

LocalClipmapHeightResidency::CacheBuffer*
LocalClipmapHeightResidency::desiredBuffer(std::uint32_t level) {
    validateLevel(level);
    if (levels_[level].staging) {
        return &*levels_[level].staging;
    }
    return levels_[level].active ? &*levels_[level].active : nullptr;
}

const LocalClipmapHeightResidency::CacheCell*
LocalClipmapHeightResidency::findCell(
        const CacheBuffer& buffer,
        const LocalClipmapGridCoordinate& coordinate) const {
    if (!coordinateInWindow(
            coordinate,
            buffer.windowMinimum,
            clipmapConfig_.samplesPerAxis)) {
        return nullptr;
    }
    const CacheCell& cell = buffer.cells[localClipmapToroidalIndex(
        coordinate,
        clipmapConfig_.samplesPerAxis)];
    return cell.coordinate == coordinate ? &cell : nullptr;
}

LocalClipmapHeightResidency::CacheCell*
LocalClipmapHeightResidency::findCell(
        CacheBuffer& buffer,
        const LocalClipmapGridCoordinate& coordinate) {
    return const_cast<CacheCell*>(
        std::as_const(*this).findCell(buffer, coordinate));
}

LocalClipmapHeightResidency::CacheBuffer
LocalClipmapHeightResidency::makeTargetBuffer(
        const LocalClipmapUpdateIdentity& identity,
        const LocalClipmapCacheOrigin& origin) const {
    CacheBuffer result{
        .identity = identity,
        .origin = origin,
        .windowMinimum = localClipmapWindowMinimum(clipmapConfig_, origin),
        .requestedAt = std::chrono::steady_clock::now(),
    };
    const std::size_t sampleCount =
        static_cast<std::size_t>(clipmapConfig_.samplesPerAxis) *
        clipmapConfig_.samplesPerAxis;
    result.cells.resize(sampleCount);
    const auto size = static_cast<std::int64_t>(clipmapConfig_.samplesPerAxis);
    for (std::int64_t y = 0; y < size; ++y) {
        for (std::int64_t x = 0; x < size; ++x) {
            const LocalClipmapGridCoordinate coordinate{
                checkedAdd(result.windowMinimum.x, x),
                checkedAdd(result.windowMinimum.y, y),
            };
            result.cells[localClipmapToroidalIndex(
                coordinate,
                clipmapConfig_.samplesPerAxis)].coordinate = coordinate;
        }
    }
    return result;
}

bool LocalClipmapHeightResidency::contentCompatible(
        const CacheBuffer& source,
        const CacheBuffer& target) const {
    return source.identity.terrainEpoch == target.identity.terrainEpoch &&
        source.identity.terrainDependencyRevision ==
            target.identity.terrainDependencyRevision &&
        source.identity.localFrameRevision ==
            target.identity.localFrameRevision &&
        source.identity.level == target.identity.level &&
        source.origin.localFrameAnchorMeters ==
            target.origin.localFrameAnchorMeters;
}

std::size_t LocalClipmapHeightResidency::reuseSamples(
        const CacheBuffer& source,
        CacheBuffer& target) const {
    std::size_t reused = 0;
    for (CacheCell& targetCell : target.cells) {
        if (targetCell.state != CellState::Missing) {
            continue;
        }
        const CacheCell* sourceCell = findCell(source, targetCell.coordinate);
        if (sourceCell == nullptr) {
            continue;
        }
        if (sourceCell->state == CellState::Resident) {
            targetCell.cpuHeightMeters = sourceCell->cpuHeightMeters;
            targetCell.gpuHeightMeters = sourceCell->gpuHeightMeters;
            targetCell.state = CellState::Resident;
            ++reused;
        } else if (sourceCell->state == CellState::CpuReady ||
                sourceCell->state == CellState::UploadPending) {
            targetCell.cpuHeightMeters = sourceCell->cpuHeightMeters;
            targetCell.state = CellState::CpuReady;
            ++reused;
        }
    }
    return reused;
}

std::vector<LocalClipmapValidRegion>
LocalClipmapHeightResidency::validRegions(const CacheBuffer& buffer) const {
    std::vector<LocalClipmapValidRegion> regions;
    const auto size = static_cast<std::int64_t>(clipmapConfig_.samplesPerAxis);
    for (std::int64_t yOffset = 0; yOffset < size; ++yOffset) {
        const std::int64_t y = checkedAdd(buffer.windowMinimum.y, yOffset);
        std::int64_t xOffset = 0;
        while (xOffset < size) {
            while (xOffset < size) {
                const LocalClipmapGridCoordinate coordinate{
                    checkedAdd(buffer.windowMinimum.x, xOffset),
                    y,
                };
                const CacheCell* cell = findCell(buffer, coordinate);
                if (cell && cell->state == CellState::Resident) {
                    break;
                }
                ++xOffset;
            }
            if (xOffset == size) {
                break;
            }
            const std::int64_t runStart = xOffset;
            while (xOffset < size) {
                const LocalClipmapGridCoordinate coordinate{
                    checkedAdd(buffer.windowMinimum.x, xOffset),
                    y,
                };
                const CacheCell* cell = findCell(buffer, coordinate);
                if (cell == nullptr || cell->state != CellState::Resident) {
                    break;
                }
                ++xOffset;
            }
            const LocalClipmapGridCoordinate minimum{
                checkedAdd(buffer.windowMinimum.x, runStart),
                y,
            };
            const auto width = static_cast<std::uint32_t>(xOffset - runStart);
            bool merged = false;
            for (LocalClipmapValidRegion& region : regions) {
                if (region.minimum.x == minimum.x &&
                        region.widthSamples == width &&
                        checkedAdd(
                            region.minimum.y,
                            region.heightSamples) == minimum.y) {
                    ++region.heightSamples;
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                regions.push_back({minimum, width, 1});
            }
        }
    }
    return regions;
}

void LocalClipmapHeightResidency::validateLevel(std::uint32_t level) const {
    if (level >= levels_.size()) {
        throw std::out_of_range{
            "local clipmap residency level is outside the configured range"};
    }
}

void LocalClipmapHeightResidency::promoteIfComplete(std::uint32_t level) {
    validateLevel(level);
    LevelState& state = levels_[level];
    if (!state.staging || !std::all_of(
            state.staging->cells.begin(),
            state.staging->cells.end(),
            [](const CacheCell& cell) {
                return cell.state == CellState::Resident;
            })) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const double latency = std::chrono::duration<double, std::milli>(
        now - state.staging->requestedAt).count();
    eraseObsoleteWork(level);
    state.active = std::move(state.staging);
    state.staging.reset();
    ++metrics_.completedUpdates;
    metrics_.lastUpdateLatencyMilliseconds = latency;
    metrics_.maximumUpdateLatencyMilliseconds = std::max(
        metrics_.maximumUpdateLatencyMilliseconds,
        latency);
}

void LocalClipmapHeightResidency::eraseObsoleteWork(std::uint32_t level) {
    std::erase_if(generationWork_, [level](const auto& entry) {
        return entry.second.level == level;
    });
    std::erase_if(uploadWork_, [level](const auto& entry) {
        return entry.second.level == level;
    });
}

std::vector<std::uint32_t>
LocalClipmapHeightResidency::prioritizedLevels() const {
    std::vector<std::uint32_t> result;
    result.reserve(levels_.size());
    for (std::uint32_t level = 0; level < levels_.size(); ++level) {
        result.push_back(level);
    }
    std::sort(result.begin(), result.end(), [this](
            std::uint32_t lhs,
            std::uint32_t rhs) {
        return localClipmapResidencyPriority(lhs, levels_.size()) <
            localClipmapResidencyPriority(rhs, levels_.size());
    });
    return result;
}

} // namespace wgen
