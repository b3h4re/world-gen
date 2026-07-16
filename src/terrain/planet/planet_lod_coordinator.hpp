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
    std::size_t patchGenerationBudget{DEFAULT_PLANET_PATCH_GENERATION_BUDGET};
    std::size_t patchUploadBudget{DEFAULT_PLANET_PATCH_UPLOAD_BUDGET};
    std::size_t maximumConcurrentPatchJobs{DEFAULT_PLANET_MAX_CONCURRENT_PATCH_JOBS};
};

struct PlanetLodTransitionConfig {
    double durationSeconds{0.25};
    double debugTimeScale{1.0};
};

enum class PlanetLodTransitionKind : std::uint8_t {
    Split,
    Merge,
};

struct PlanetLodTransitionState {
    PlanetPatchId parent{};
    PlanetLodTransitionKind kind{PlanetLodTransitionKind::Split};
    double childMorph{};

    bool operator==(const PlanetLodTransitionState&) const = default;
};

struct PlanetPatchDrawState {
    PlanetPatchId id{};
    float morph{1.0F};
    PlanetPatchEdgeMask stitchMask{};

    bool operator==(const PlanetPatchDrawState&) const = default;
};

struct PlanetLodPatchPlan {
    std::vector<PlanetPatchId> upserts{};
    std::vector<PlanetPatchId> removals{};
};

struct PlanetPatchErrorSample {
    PlanetPatchId id{};
    double maximumWorldError{};
};

void validatePlanetLodTransitionConfig(const PlanetLodTransitionConfig& config);

PlanetPatchEdgeMask planetPatchStitchMask(
    const PlanetPatchId& id,
    std::span<const PlanetPatchId> activeLeaves);

class PlanetLodCoordinator {
public:
    PlanetLodCoordinator(
        PlanetLodConfig lodConfig = {},
        PlanetLodStreamingConfig streamingConfig = {},
        PlanetLodTransitionConfig transitionConfig = {});

    void reset();
    void setMaximumLevel(std::uint8_t maximumLevel);
    void setSurface(PlanetLodSurface surface);
    bool updateSelection(const PlanetLodView& view);
    void updateVisibility(const PlanetLodView& view);
    void advanceTransitions(double deltaSeconds);
    void setTransitionDebugTimeScale(double timeScale);

    PlanetLodPatchPlan makePatchPlan() const;
    void beginPatchPlan(
        const PlanetLodPatchPlan& plan,
        std::uint64_t requestRevision = 1);
    void cancelPendingRequests();
    void cancelPendingRequestsBefore(std::uint64_t requestRevision);
    void cancelPatchPlan(
        const PlanetLodPatchPlan& plan,
        std::uint64_t requestRevision);
    bool publishPatchChanges(
        std::span<const PlanetPatchId> upserts,
        std::span<const PlanetPatchId> removals,
        std::uint64_t requestRevision = 0,
        std::span<const PlanetPatchErrorSample> errorSamples = {});

    const PlanetLodConfig& lodConfig() const { return lodConfig_; }
    const PlanetLodStreamingConfig& streamingConfig() const { return streamingConfig_; }
    const PlanetLodTransitionConfig& transitionConfig() const { return transitionConfig_; }
    const std::vector<PlanetPatchId>& desiredLeaves() const { return desiredLeaves_; }
    const std::vector<PlanetPatchId>& activeLeaves() const { return activeLeaves_; }
    const std::vector<PlanetPatchId>& visibleActiveLeaves() const { return visibleActiveLeaves_; }
    const std::vector<PlanetPatchDrawState>& visibleDrawStates() const {
        return visibleDrawStates_;
    }
    const std::vector<PlanetLodTransitionState>& transitions() const { return transitions_; }
    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& residentIds() const {
        return residentIds_;
    }
    const PlanetPatchErrorCache& knownPatchErrors() const { return knownPatchErrors_; }
    std::size_t pendingPatchCount() const {
        return pendingUpserts_.size() + pendingRemovals_.size();
    }
    bool hasPendingRequests() const { return pendingPatchCount() != 0; }

private:
    void reconcileActiveLeaves();
    void rebuildVisibleActiveLeaves();
    bool transitionLocks(const PlanetPatchId& id) const;
    std::vector<PlanetPatchId> protectedCacheIds() const;
    void recordView(const PlanetLodView& view);
    void pruneKnownPatchErrors();

    PlanetLodSelector selector_{};
    PlanetLodConfig lodConfig_{};
    PlanetLodStreamingConfig streamingConfig_{};
    PlanetLodTransitionConfig transitionConfig_{};
    PlanetLodSurface surface_{};
    std::vector<PlanetPatchId> desiredLeaves_{};
    std::vector<PlanetPatchId> activeLeaves_{};
    std::vector<PlanetPatchId> visibleActiveLeaves_{};
    std::vector<PlanetPatchDrawState> visibleDrawStates_{};
    std::vector<PlanetLodTransitionState> transitions_{};
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> residentIds_{};
    std::unordered_map<PlanetPatchId, std::uint64_t, PlanetPatchIdHash> pendingUpserts_{};
    std::unordered_map<PlanetPatchId, std::uint64_t, PlanetPatchIdHash> pendingRemovals_{};
    PlanetPatchErrorCache knownPatchErrors_{};
    std::unordered_map<PlanetPatchId, std::uint64_t, PlanetPatchIdHash> lastUsedTick_{};
    std::optional<PlanetLodView> latestView_{};
    std::optional<PlanetLodView> previousView_{};
    std::uint64_t minimumAcceptedRevision_{0};
    std::uint64_t selectionTick_{0};
};

} // namespace wgen
