#include "terrain_app_renderer.hpp"

#include "renderer/objects/mesh_2d.hpp"
#include "renderer/objects/mesh_3d.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan.h>

namespace lve {

TerrainAppRenderer::TerrainAppRenderer() : TerrainAppRenderer(wgen::WindowConfig{}) {}

TerrainAppRenderer::TerrainAppRenderer(const wgen::WindowConfig& config)
    : window_{config.width, config.height, "World Generator"}, device_{window_},
    renderer_{window_, device_, config.present_mode}, colorMapper_{device_} {
    initDescriptorPool();
    window_.setSurfaceAboutToBeDestroyedCallback([this] {
        shutdownVulkanResources();
    });
}

TerrainAppRenderer::~TerrainAppRenderer() {
    window_.setSurfaceAboutToBeDestroyedCallback({});
    shutdownVulkanResources();
}

void TerrainAppRenderer::setDesiredPresentMode(PresentMode desiredPresentMode) {
    desiredPresentMode_ = desiredPresentMode;
    renderer_.setDesiredPresentMode(desiredPresentMode);
}

void TerrainAppRenderer::applyTerrainMesh(int frameIndex, TerrainPlaneMeshData data) {
    if (frameIndex < 0 || frameIndex >= static_cast<int>(retiredObjects_.size())) {
        throw std::out_of_range{"terrain mesh frame index is out of range"};
    }

    std::vector<GameObject2d> newObjects2d = makeObjects2d(data);
    std::vector<GameObject3d> newObjects3d = makeObjects3d(data);
    retiredObjects_[frameIndex].objects2d = std::move(objects2d_);
    retiredObjects_[frameIndex].objects3d = std::move(objects3d_);
    objects2d_ = std::move(newObjects2d);
    objects3d_ = std::move(newObjects3d);
}

void TerrainAppRenderer::applyPlanetPatchBatch(
        int frameIndex,
        PlanetPatchMeshBatch batch) {
    if (frameIndex < 0 || frameIndex >= static_cast<int>(retiredObjects_.size())) {
        throw std::out_of_range{"planet patch batch frame index is out of range"};
    }
    validatePlanetPatchMeshBatch(batch);
    const PlanetPatchBatchEpochAction epochAction = planetPatchBatchEpochAction(
        batch.terrainEpoch,
        activePlanetEpoch_);
    if (epochAction == PlanetPatchBatchEpochAction::Discard) {
        return;
    }

    const bool replacesEpoch = epochAction == PlanetPatchBatchEpochAction::Replace;
    std::vector<const PlanetPatchMeshData*> acceptedUpserts;
    acceptedUpserts.reserve(batch.upserts.size());
    for (const PlanetPatchMeshData& upsert : batch.upserts) {
        const auto resident = residentPlanetPatches_.find(upsert.id);
        if (!replacesEpoch && resident != residentPlanetPatches_.end() &&
                !shouldApplyPlanetPatchUpsert(upsert.version, resident->second.version)) {
            continue;
        }
        acceptedUpserts.push_back(&upsert);
    }

    struct PreparedPlanetPatch {
        wgen::PlanetPatchId id{};
        ResidentPlanetPatch resident{};
    };
    std::vector<PreparedPlanetPatch> preparedUpserts;
    preparedUpserts.reserve(acceptedUpserts.size());
    for (const PlanetPatchMeshData* upsert : acceptedUpserts) {
        if (planetPatchIndexSet_ == nullptr ||
                planetPatchIndexQuadCount_ != upsert->quadCount) {
            if (planetPatchIndexSet_ != nullptr && !replacesEpoch) {
                throw std::invalid_argument{"planet patch quad count changed within an epoch"};
            }
            const PlanetPatchIndexVariants variants =
                buildPlanetPatchIndexVariants(upsert->quadCount);
            planetPatchIndexSet_ = std::make_shared<Mesh3dIndexSet>(
                device_,
                std::span<const std::vector<std::uint32_t>>{
                    variants.data(),
                    variants.size()});
            planetPatchIndexQuadCount_ = upsert->quadCount;
        }
        auto mesh = std::make_shared<Mesh3d>(
            device_,
            upsert->vertices,
            planetPatchIndexSet_);
        preparedUpserts.push_back({
            .id = upsert->id,
            .resident = {
                .object = {
                    .mesh = std::move(mesh),
                    .globalOrigin = upsert->globalOrigin,
                },
                .version = upsert->version,
            },
        });
    }

    decltype(residentPlanetPatches_) nextResidents;
    if (!replacesEpoch) {
        nextResidents = residentPlanetPatches_;
    }
    for (const PlanetPatchRemoval& removal : batch.removals) {
        const auto resident = nextResidents.find(removal.id);
        if (resident != nextResidents.end() &&
                shouldApplyPlanetPatchRemoval(removal, resident->second.version)) {
            nextResidents.erase(resident);
        }
    }
    for (PreparedPlanetPatch& upsert : preparedUpserts) {
        nextResidents.insert_or_assign(upsert.id, std::move(upsert.resident));
    }

    std::vector<wgen::PlanetPatchDrawState> nextDrawOrder;
    if (!replacesEpoch) {
        nextDrawOrder.reserve(planetDrawOrder_.size());
        for (const wgen::PlanetPatchDrawState& state : planetDrawOrder_) {
            if (nextResidents.contains(state.id)) {
                nextDrawOrder.push_back(state);
            }
        }
    }

    std::vector<GameObject3d> nextObjectsPlanet;
    nextObjectsPlanet.reserve(nextDrawOrder.size());
    for (const wgen::PlanetPatchDrawState& state : nextDrawOrder) {
        GameObject3d object = nextResidents.at(state.id).object;
        object.terrainMorph = state.morph;
        object.meshIndexVariant = state.stitchMask;
        nextObjectsPlanet.push_back(std::move(object));
    }

    std::vector<wgen::PlanetPatchId> retiredIds;
    retiredIds.reserve(residentPlanetPatches_.size());
    for (const auto& [id, resident] : residentPlanetPatches_) {
        const auto next = nextResidents.find(id);
        if (next == nextResidents.end() || next->second.object.mesh != resident.object.mesh) {
            retiredIds.push_back(id);
        }
    }

    std::vector<GameObject3d>& retired = retiredObjects_[frameIndex].objectsPlanet;
    retired.reserve(retired.size() + retiredIds.size());
    residentPlanetPatches_.swap(nextResidents);
    planetDrawOrder_.swap(nextDrawOrder);
    objectsPlanet_.swap(nextObjectsPlanet);
    for (const wgen::PlanetPatchId& id : retiredIds) {
        retired.push_back(std::move(nextResidents.at(id).object));
    }
    activePlanetEpoch_ = batch.terrainEpoch;
}

void TerrainAppRenderer::setPlanetDrawPatches(
        std::span<const wgen::PlanetPatchDrawState> states) {
    std::vector<wgen::PlanetPatchDrawState> nextDrawOrder{states.begin(), states.end()};
    for (const wgen::PlanetPatchDrawState& state : nextDrawOrder) {
        wgen::validate(state.id);
        if (!std::isfinite(state.morph) || state.morph < 0.0F || state.morph > 1.0F ||
                state.stitchMask >= wgen::PLANET_PATCH_EDGE_MASK_COUNT) {
            throw std::invalid_argument{"planet patch draw state is invalid"};
        }
        const auto resident = residentPlanetPatches_.find(state.id);
        if (resident == residentPlanetPatches_.end() ||
                resident->second.version.terrainEpoch != activePlanetEpoch_) {
            throw std::invalid_argument{"planet draw patch is not resident in the active epoch"};
        }
    }
    std::sort(nextDrawOrder.begin(), nextDrawOrder.end(), [](const auto& lhs, const auto& rhs) {
        return wgen::PlanetPatchIdLess{}(lhs.id, rhs.id);
    });
    if (std::adjacent_find(
            nextDrawOrder.begin(),
            nextDrawOrder.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.id == rhs.id; }) !=
            nextDrawOrder.end()) {
        throw std::invalid_argument{"planet draw patch list contains duplicate IDs"};
    }

    std::vector<GameObject3d> nextObjectsPlanet;
    nextObjectsPlanet.reserve(nextDrawOrder.size());
    for (const wgen::PlanetPatchDrawState& state : nextDrawOrder) {
        GameObject3d object = residentPlanetPatches_.at(state.id).object;
        object.terrainMorph = state.morph;
        object.meshIndexVariant = state.stitchMask;
        nextObjectsPlanet.push_back(std::move(object));
    }
    planetDrawOrder_.swap(nextDrawOrder);
    objectsPlanet_.swap(nextObjectsPlanet);
}

void TerrainAppRenderer::clearRetiredObjects(int frameIndex) {
    retiredObjects_[frameIndex].clear();
}

void TerrainAppRenderer::waitIdle() {
    vkDeviceWaitIdle(device_.device());
}

void TerrainAppRenderer::shutdownVulkanResources() {
    if (vulkanResourcesShutdown_) {
        return;
    }

    vulkanResourcesShutdown_ = true;
    if (renderer_.isFrameInProgress()) {
        renderer_.abortFrame();
    }

    waitIdle();
    retiredObjects_ = {};
    objects3d_.clear();
    objects2d_.clear();
    objectsPlanet_.clear();
    planetDrawOrder_.clear();
    residentPlanetPatches_.clear();
    planetPatchIndexSet_.reset();
    planetPatchIndexQuadCount_ = 0;
    activePlanetEpoch_ = 0;
    globalPool_.reset();
    renderer_.destroySwapChain();
}

void TerrainAppRenderer::initDescriptorPool() {
    globalPool_ = LveDescriptorPool::Builder(device_)
        .setMaxSets(100)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LveSwapChain::MAX_FRAMES_IN_FLIGHT + 1)
        .build();
}

std::vector<GameObject2d> TerrainAppRenderer::makeObjects2d(const TerrainPlaneMeshData& data) {
    std::vector<GameObject2d> objects;
    auto mesh = std::make_shared<Mesh2d>(device_, data.vertices2d, data.indices2d);
    objects.push_back({std::move(mesh), {}});
    return objects;
}

std::vector<GameObject3d> TerrainAppRenderer::makeObjects3d(const TerrainPlaneMeshData& data) {
    std::vector<GameObject3d> objects;
    auto mesh = std::make_shared<Mesh3d>(device_, data.vertices3d, data.indices3d);
    objects.push_back({std::move(mesh), {}});
    return objects;
}

} // namespace lve
