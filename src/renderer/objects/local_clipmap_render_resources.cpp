#include "renderer/objects/local_clipmap_render_resources.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace lve {
namespace {

bool sameHeightContent(
        const wgen::LocalClipmapUpdateIdentity& lhs,
        const wgen::LocalClipmapUpdateIdentity& rhs) {
    return lhs.terrainEpoch == rhs.terrainEpoch &&
        lhs.terrainDependencyRevision == rhs.terrainDependencyRevision &&
        lhs.localFrameRevision == rhs.localFrameRevision &&
        lhs.level == rhs.level &&
        lhs.snappedGridOrigin == rhs.snappedGridOrigin;
}

} // namespace

LocalClipmapRenderResources::LocalClipmapRenderResources(
        LveDevice& device,
        wgen::LocalClipmapConfig config)
    : device_{device},
      config_{std::move(config)},
      topology_{wgen::buildLocalClipmapTopology(config_)} {
    std::array<std::vector<std::uint32_t>, 4> variants{
        topology_.centerPattern.indices,
        topology_.ringPattern.indices,
        triangleEdges(topology_.centerPattern.indices),
        triangleEdges(topology_.ringPattern.indices),
    };
    indexSet_ = std::make_shared<Mesh3dIndexSet>(
        device_,
        std::span<const std::vector<std::uint32_t>>{
            variants.data(),
            variants.size()});

    const std::uint64_t sampleCount64 =
        std::uint64_t{config_.samplesPerAxis} * config_.samplesPerAxis;
    if (sampleCount64 > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error{
            "local clipmap height buffer exceeds uint32 capacity"};
    }
    const auto sampleCount = static_cast<std::uint32_t>(sampleCount64);
    heightBuffers_.reserve(config_.levelCount);
    for (std::uint32_t level = 0; level < config_.levelCount; ++level) {
        auto buffer = std::make_unique<LveBuffer>(
            device_,
            sizeof(float),
            sampleCount,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (buffer->map() != VK_SUCCESS) {
            throw std::runtime_error{
                "failed to map local clipmap height buffer"};
        }
        heightBuffers_.push_back(std::move(buffer));
    }
    heightBufferIdentities_.resize(config_.levelCount);
    residentLevels_.resize(config_.levelCount);
}

void LocalClipmapRenderResources::applyHeightUpload(
        const wgen::LocalClipmapGpuUploadBatch& batch) {
    wgen::validateLocalClipmapUpdateIdentity(batch.identity, config_);
    if (batch.uploadId == 0 || batch.writes.empty()) {
        throw std::invalid_argument{
            "local clipmap renderer upload is empty or uninitialized"};
    }
    LveBuffer& heightBuffer = *heightBuffers_.at(batch.identity.level);
    for (const wgen::LocalClipmapGpuHeightWrite& write : batch.writes) {
        if (write.toroidalIndex >= heightBuffer.getInstanceCount() ||
                write.toroidalIndex != wgen::localClipmapToroidalIndex(
                    write.coordinate,
                    config_.samplesPerAxis)) {
            throw std::invalid_argument{
                "local clipmap renderer upload has an invalid address"};
        }
        float height = write.heightMeters;
        heightBuffer.writeToIndex(
            &height,
            static_cast<int>(write.toroidalIndex));
    }
    heightBufferIdentities_[batch.identity.level] = batch.identity;
}

void LocalClipmapRenderResources::applyMeshUpdate(
        LocalClipmapMeshUpdate update,
        std::vector<GameObject3d>& retiredObjects) {
    for (LocalClipmapLevelMeshData& upsert : update.upserts) {
        wgen::validateLocalClipmapUpdateIdentity(upsert.identity, config_);
        if (upsert.level != upsert.identity.level ||
                upsert.vertices.size() != topology_.vertices.size()) {
            throw std::invalid_argument{
                "local clipmap renderer mesh update is inconsistent"};
        }
        const auto heightIdentity = heightBufferIdentities_[upsert.level];
        if (!heightIdentity ||
                !sameHeightContent(*heightIdentity, upsert.identity)) {
            throw std::invalid_argument{
                "local clipmap renderer mesh precedes its height upload"};
        }

        auto mesh = std::make_shared<Mesh3d>(
            device_,
            upsert.vertices,
            indexSet_);
        GameObject3d object{
            .mesh = std::move(mesh),
            .globalOrigin = upsert.globalOrigin,
            .meshIndexVariant = upsert.level == 0
                ? LOCAL_CLIPMAP_CENTER_TRIANGLES_INDEX_VARIANT
                : LOCAL_CLIPMAP_RING_TRIANGLES_INDEX_VARIANT,
        };
        std::optional<ResidentLevel>& resident =
            residentLevels_[upsert.level];
        if (resident) {
            retiredObjects.push_back(std::move(resident->object));
        }
        resident = ResidentLevel{
            .object = std::move(object),
            .identity = upsert.identity,
        };
    }

    for (const LocalClipmapLevelCacheState& state : update.cacheStates) {
        if (state.level >= residentLevels_.size()) {
            throw std::out_of_range{
                "local clipmap cache state level is out of range"};
        }
        std::optional<ResidentLevel>& resident = residentLevels_[state.level];
        if (!state.hasActiveCache && resident) {
            retiredObjects.push_back(std::move(resident->object));
            resident.reset();
        } else if (state.hasActiveCache && !resident) {
            throw std::invalid_argument{
                "local clipmap renderer cache state disagrees with residency"};
        }
        if (resident) {
            resident->cacheCurrent = state.activeCacheIsCurrent;
        }
    }
    rebuildObjects();
}

void LocalClipmapRenderResources::clear() {
    objects_.clear();
    residentLevels_.clear();
    heightBufferIdentities_.clear();
    heightBuffers_.clear();
    indexSet_.reset();
}

std::vector<std::uint32_t> LocalClipmapRenderResources::triangleEdges(
        std::span<const std::uint32_t> triangles) {
    if (triangles.empty() || triangles.size() % 3 != 0) {
        throw std::invalid_argument{
            "local clipmap line topology requires complete triangles"};
    }
    std::vector<std::uint32_t> result;
    result.reserve(triangles.size() * 2);
    for (std::size_t index = 0; index < triangles.size(); index += 3) {
        const std::uint32_t a = triangles[index];
        const std::uint32_t b = triangles[index + 1];
        const std::uint32_t c = triangles[index + 2];
        result.insert(result.end(), {a, b, b, c, c, a});
    }
    return result;
}

void LocalClipmapRenderResources::rebuildObjects() {
    objects_.clear();
    objects_.reserve(residentLevels_.size());
    for (std::uint32_t level = 0; level < residentLevels_.size(); ++level) {
        const std::optional<ResidentLevel>& resident = residentLevels_[level];
        if (!resident) {
            continue;
        }
        objects_.push_back({
            .object = resident->object,
            .level = level,
            .cacheCurrent = resident->cacheCurrent,
        });
    }
}

} // namespace lve
