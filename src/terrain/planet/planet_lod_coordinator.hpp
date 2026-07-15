#pragma once

#include "terrain/planet/planet_lod_selector.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wgen {

struct PlanetLodStreamingConfig {
    std::size_t residentPatchBudget{DEFAULT_PLANET_RESIDENT_PATCH_BUDGET};
    std::size_t patchUploadBudget{DEFAULT_PLANET_PATCH_UPLOAD_BUDGET};
};

struct PlanetLodPatchPlan {
    std::vector<PlanetPatchId> upserts{};
    std::vector<PlanetPatchId> removals{};
};

class PlanetLodCoordinator {
public:
    PlanetLodCoordinator(
        PlanetLodConfig lodConfig = {},
        PlanetLodStreamingConfig streamingConfig = {});

    void reset();
    void setMaximumLevel(std::uint8_t maximumLevel);
    void setSurface(PlanetLodSurface surface);
    bool updateSelection(const PlanetLodView& view);
    void updateVisibility(const PlanetLodView& view);

    PlanetLodPatchPlan makePatchPlan() const;
    void beginPatchPlan(const PlanetLodPatchPlan& plan);
    void cancelPendingRequests();
    void publishPatchChanges(
        std::span<const PlanetPatchId> upserts,
        std::span<const PlanetPatchId> removals);

    const PlanetLodConfig& lodConfig() const { return lodConfig_; }
    const PlanetLodStreamingConfig& streamingConfig() const { return streamingConfig_; }
    const std::vector<PlanetPatchId>& desiredLeaves() const { return desiredLeaves_; }
    const std::vector<PlanetPatchId>& activeLeaves() const { return activeLeaves_; }
    const std::vector<PlanetPatchId>& visibleActiveLeaves() const { return visibleActiveLeaves_; }
    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& residentIds() const {
        return residentIds_;
    }
    bool hasPendingRequests() const { return !pendingIds_.empty(); }

private:
    void reconcileActiveLeaves();
    void rebuildVisibleActiveLeaves();
    std::vector<PlanetPatchId> protectedCacheIds() const;

    PlanetLodSelector selector_{};
    PlanetLodConfig lodConfig_{};
    PlanetLodStreamingConfig streamingConfig_{};
    PlanetLodSurface surface_{};
    std::vector<PlanetPatchId> desiredLeaves_{};
    std::vector<PlanetPatchId> activeLeaves_{};
    std::vector<PlanetPatchId> visibleActiveLeaves_{};
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> residentIds_{};
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> pendingIds_{};
    std::unordered_map<PlanetPatchId, std::uint64_t, PlanetPatchIdHash> lastUsedTick_{};
    std::optional<PlanetLodView> latestView_{};
    std::uint64_t selectionTick_{0};
};

} // namespace wgen
