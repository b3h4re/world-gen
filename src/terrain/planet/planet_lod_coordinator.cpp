#include "planet_lod_coordinator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

namespace wgen {
namespace {

constexpr std::array PLANET_PATCH_EDGES{
    PlanetPatchEdge::UMin,
    PlanetPatchEdge::UMax,
    PlanetPatchEdge::VMin,
    PlanetPatchEdge::VMax,
};

std::vector<PlanetPatchId> rootPatchIds() {
    std::vector<PlanetPatchId> result;
    result.reserve(FACES.size());
    for (const CubeSphereFace face : FACES) {
        result.push_back({face, 0, 0, 0});
    }
    return result;
}

bool isDescendantOrSelf(const PlanetPatchId& id, const PlanetPatchId& ancestor) {
    if (id.face != ancestor.face || id.level < ancestor.level) {
        return false;
    }
    const std::uint8_t difference = static_cast<std::uint8_t>(id.level - ancestor.level);
    return (id.x >> difference) == ancestor.x && (id.y >> difference) == ancestor.y;
}

bool hasDescendant(
        const PlanetPatchId& ancestor,
        std::span<const PlanetPatchId> leaves) {
    return std::any_of(leaves.begin(), leaves.end(), [&ancestor](const PlanetPatchId& leaf) {
        return leaf.level > ancestor.level && isDescendantOrSelf(leaf, ancestor);
    });
}

std::optional<PlanetPatchId> selectedCoveringAncestor(
        PlanetPatchId id,
        const std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& leaves) {
    while (true) {
        if (leaves.contains(id)) {
            return id;
        }
        const std::optional<PlanetPatchId> next = parent(id);
        if (!next) {
            return std::nullopt;
        }
        id = *next;
    }
}

bool leavesAreBalanced(
        const std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& leaves) {
    for (const PlanetPatchId& leaf : leaves) {
        for (const PlanetPatchEdge edge : PLANET_PATCH_EDGES) {
            const PlanetPatchId sameLevel = sameLevelNeighbor(leaf, edge).id;
            const std::optional<PlanetPatchId> neighbor = selectedCoveringAncestor(sameLevel, leaves);
            if (neighbor && neighbor->level + 1 < leaf.level) {
                return false;
            }
        }
    }
    return true;
}

bool containsAll(
        const std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& ids,
        const std::array<PlanetPatchId, 4>& required) {
    return std::all_of(required.begin(), required.end(), [&ids](const PlanetPatchId& id) {
        return ids.contains(id);
    });
}

std::vector<PlanetPatchId> sortedIds(
        const std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& ids) {
    std::vector<PlanetPatchId> result{ids.begin(), ids.end()};
    std::sort(result.begin(), result.end(), PlanetPatchIdLess{});
    return result;
}

bool desiredIsCoarserThan(
        const PlanetPatchId& parentId,
        std::span<const PlanetPatchId> desiredLeaves) {
    return std::any_of(
        desiredLeaves.begin(),
        desiredLeaves.end(),
        [&parentId](const PlanetPatchId& desired) {
            return isDescendantOrSelf(parentId, desired);
        });
}

PlanetPatchEdgeMask stitchMaskFromActiveSet(
        const PlanetPatchId& id,
        const std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& active) {
    PlanetPatchEdgeMask result = 0;
    for (const PlanetPatchEdge edge : PLANET_PATCH_EDGES) {
        const PlanetPatchId sameLevel = sameLevelNeighbor(id, edge).id;
        const std::optional<PlanetPatchId> neighbor = selectedCoveringAncestor(
            sameLevel,
            active);
        if (neighbor && neighbor->level + 1 == id.level) {
            result |= planetPatchEdgeBit(edge);
        }
    }
    return result;
}

bool sameVector(glm::dvec3 lhs, glm::dvec3 rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameView(const PlanetLodView& lhs, const PlanetLodView& rhs) {
    return sameVector(lhs.position, rhs.position) &&
        sameVector(lhs.forward, rhs.forward) &&
        sameVector(lhs.right, rhs.right) &&
        sameVector(lhs.up, rhs.up) &&
        lhs.verticalFov == rhs.verticalFov &&
        lhs.aspectRatio == rhs.aspectRatio &&
        lhs.nearPlane == rhs.nearPlane &&
        lhs.farPlane == rhs.farPlane &&
        lhs.viewportHeight == rhs.viewportHeight;
}

} // namespace

void validatePlanetLodTransitionConfig(const PlanetLodTransitionConfig& config) {
    if (!std::isfinite(config.durationSeconds) || config.durationSeconds <= 0.0 ||
            !std::isfinite(config.debugTimeScale) || config.debugTimeScale < 0.0) {
        throw std::invalid_argument{"planet LOD transition config is invalid"};
    }
}

PlanetPatchEdgeMask planetPatchStitchMask(
        const PlanetPatchId& id,
        std::span<const PlanetPatchId> activeLeaves) {
    validate(id);
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> active;
    active.reserve(activeLeaves.size());
    for (const PlanetPatchId& leaf : activeLeaves) {
        validate(leaf);
        active.insert(leaf);
    }
    return stitchMaskFromActiveSet(id, active);
}

PlanetLodCoordinator::PlanetLodCoordinator(
        PlanetLodConfig lodConfig,
        PlanetLodStreamingConfig streamingConfig,
        PlanetLodTransitionConfig transitionConfig)
    : lodConfig_{lodConfig},
      streamingConfig_{streamingConfig},
      transitionConfig_{transitionConfig} {
    validatePlanetLodConfig(lodConfig_);
    validatePlanetLodTransitionConfig(transitionConfig_);
    if (streamingConfig_.residentPatchBudget < FACES.size() ||
            streamingConfig_.patchGenerationBudget < 4 ||
            streamingConfig_.patchUploadBudget == 0 ||
            streamingConfig_.patchUploadBudget > streamingConfig_.residentPatchBudget ||
            streamingConfig_.maximumConcurrentPatchJobs == 0) {
        throw std::invalid_argument{"planet LOD streaming budgets are invalid"};
    }
    if (lodConfig_.selectedLeafBudget > streamingConfig_.residentPatchBudget ||
            streamingConfig_.patchGenerationBudget >
                (streamingConfig_.residentPatchBudget - lodConfig_.selectedLeafBudget) /
                    streamingConfig_.maximumConcurrentPatchJobs) {
        throw std::invalid_argument{"planet LOD selection leaves no streaming headroom"};
    }
    reset();
}

void PlanetLodCoordinator::reset() {
    desiredLeaves_ = rootPatchIds();
    activeLeaves_.clear();
    visibleActiveLeaves_.clear();
    visibleDrawStates_.clear();
    transitions_.clear();
    residentIds_.clear();
    pendingUpserts_.clear();
    pendingRemovals_.clear();
    knownPatchErrors_.clear();
    lastUsedTick_.clear();
    latestView_.reset();
    previousView_.reset();
    minimumAcceptedRevision_ = 0;
    selectionTick_ = 0;
}

void PlanetLodCoordinator::setMaximumLevel(std::uint8_t maximumLevel) {
    PlanetLodConfig updated = lodConfig_;
    updated.maximumLevel = maximumLevel;
    validatePlanetLodConfig(updated);
    lodConfig_ = updated;
}

void PlanetLodCoordinator::setSurface(PlanetLodSurface surface) {
    validatePlanetLodSurface(surface);
    surface_ = surface;
}

bool PlanetLodCoordinator::updateSelection(const PlanetLodView& view) {
    validatePlanetLodView(view);
    recordView(view);
    ++selectionTick_;
    const PlanetLodSelection selection = selector_.select(
        view,
        surface_,
        lodConfig_,
        desiredLeaves_,
        knownPatchErrors_);
    const bool changed = selection.leaves != desiredLeaves_;
    desiredLeaves_ = selection.leaves;
    for (const PlanetPatchId& id : selection.visibleLeaves) {
        lastUsedTick_[id] = selectionTick_;
    }
    reconcileActiveLeaves();
    rebuildVisibleActiveLeaves();
    return changed;
}

void PlanetLodCoordinator::updateVisibility(const PlanetLodView& view) {
    validatePlanetLodView(view);
    recordView(view);
    ++selectionTick_;
    rebuildVisibleActiveLeaves();
    for (const PlanetPatchId& id : visibleActiveLeaves_) {
        lastUsedTick_[id] = selectionTick_;
    }
}

void PlanetLodCoordinator::setTransitionDebugTimeScale(double timeScale) {
    PlanetLodTransitionConfig updated = transitionConfig_;
    updated.debugTimeScale = timeScale;
    validatePlanetLodTransitionConfig(updated);
    transitionConfig_ = updated;
}

void PlanetLodCoordinator::advanceTransitions(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        throw std::invalid_argument{"planet LOD transition delta must be finite and non-negative"};
    }
    if (transitions_.empty() || deltaSeconds == 0.0 ||
            transitionConfig_.debugTimeScale == 0.0) {
        return;
    }

    const double progressDelta = deltaSeconds * transitionConfig_.debugTimeScale /
        transitionConfig_.durationSeconds;
    for (PlanetLodTransitionState& transition : transitions_) {
        if (transition.kind == PlanetLodTransitionKind::Split) {
            transition.childMorph = std::min(1.0, transition.childMorph + progressDelta);
        } else {
            transition.childMorph = std::max(0.0, transition.childMorph - progressDelta);
        }
    }

    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> active{
        activeLeaves_.begin(),
        activeLeaves_.end(),
    };
    for (const PlanetLodTransitionState& transition : transitions_) {
        if (transition.kind != PlanetLodTransitionKind::Merge ||
                transition.childMorph > 0.0) {
            continue;
        }
        const std::array<PlanetPatchId, 4> childIds = children(transition.parent);
        for (const PlanetPatchId& childId : childIds) {
            active.erase(childId);
        }
        active.insert(transition.parent);
    }
    std::erase_if(transitions_, [](const PlanetLodTransitionState& transition) {
        return (transition.kind == PlanetLodTransitionKind::Split &&
                transition.childMorph >= 1.0) ||
            (transition.kind == PlanetLodTransitionKind::Merge &&
                transition.childMorph <= 0.0);
    });
    activeLeaves_ = sortedIds(active);
    reconcileActiveLeaves();
    rebuildVisibleActiveLeaves();
}

PlanetLodPatchPlan PlanetLodCoordinator::makePatchPlan() const {
    struct RequestGroup {
        bool visibleReplacement{};
        double screenError{};
        bool predictedVisible{};
        bool merge{};
        std::uint8_t level{};
        PlanetPatchId anchor{};
        std::vector<PlanetPatchId> ids{};
    };
    std::vector<RequestGroup> groups;
    const std::size_t maximumPendingUpserts =
        streamingConfig_.patchGenerationBudget *
        streamingConfig_.maximumConcurrentPatchJobs;
    if (pendingUpserts_.size() >= maximumPendingUpserts) {
        return {};
    }
    const std::size_t requestBudget = std::min(
        streamingConfig_.patchGenerationBudget,
        maximumPendingUpserts - pendingUpserts_.size());
    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash> active{
        activeLeaves_.begin(),
        activeLeaves_.end(),
    };
    std::optional<PlanetLodView> predictedView;
    if (latestView_) {
        predictedView = *latestView_;
        if (previousView_) {
            predictedView->position += latestView_->position - previousView_->position;
        }
    }
    const auto appendGroup = [this, &groups, &predictedView](RequestGroup group) {
        if (latestView_) {
            group.visibleReplacement = isPlanetPatchVisible(
                group.anchor,
                *latestView_,
                surface_);
            group.screenError = planetPatchScreenErrorPixels(
                group.anchor,
                *latestView_,
                surface_,
                lodConfig_.patchQuadCount,
                knownPatchErrors_.contains(group.anchor)
                    ? knownPatchErrors_.at(group.anchor)
                    : 0.0);
        }
        if (predictedView) {
            group.predictedVisible = isPlanetPatchVisible(
                group.anchor,
                *predictedView,
                surface_);
        }
        groups.push_back(std::move(group));
    };

    for (const PlanetPatchId& leaf : activeLeaves_) {
        const std::optional<PlanetPatchId> parentId = parent(leaf);
        if (!parentId || !isDescendantOrSelf(leaf, *parentId)) {
            continue;
        }
        const std::array<PlanetPatchId, 4> siblings = children(*parentId);
        if (leaf != siblings.front() || !containsAll(active, siblings)) {
            continue;
        }
        const bool desiredCoarser = std::any_of(
            desiredLeaves_.begin(),
            desiredLeaves_.end(),
            [&parentId](const PlanetPatchId& desired) {
                return isDescendantOrSelf(*parentId, desired);
            });
        if (desiredCoarser && !residentIds_.contains(*parentId) &&
                !pendingUpserts_.contains(*parentId)) {
            appendGroup({
                .merge = true,
                .level = leaf.level,
                .anchor = *parentId,
                .ids = {*parentId},
            });
        }
    }

    for (const PlanetPatchId& leaf : activeLeaves_) {
        if (!hasDescendant(leaf, desiredLeaves_)) {
            continue;
        }
        std::vector<PlanetPatchId> missing;
        for (const PlanetPatchId& childId : children(leaf)) {
            if (!residentIds_.contains(childId) &&
                    !pendingUpserts_.contains(childId)) {
                missing.push_back(childId);
            }
        }
        if (!missing.empty()) {
            appendGroup({
                .merge = false,
                .level = leaf.level,
                .anchor = leaf,
                .ids = std::move(missing),
            });
        }
    }

    std::sort(groups.begin(), groups.end(), [](const RequestGroup& lhs, const RequestGroup& rhs) {
        if (lhs.visibleReplacement != rhs.visibleReplacement) {
            return lhs.visibleReplacement;
        }
        if (lhs.screenError != rhs.screenError) {
            return lhs.screenError > rhs.screenError;
        }
        if (lhs.predictedVisible != rhs.predictedVisible) {
            return lhs.predictedVisible;
        }
        if (lhs.merge != rhs.merge) {
            return lhs.merge;
        }
        if (lhs.level != rhs.level) {
            return lhs.merge ? lhs.level > rhs.level : lhs.level < rhs.level;
        }
        return PlanetPatchIdLess{}(lhs.anchor, rhs.anchor);
    });

    PlanetLodPatchPlan plan;
    for (const RequestGroup& group : groups) {
        if (plan.upserts.size() + group.ids.size() > requestBudget) {
            continue;
        }
        plan.upserts.insert(plan.upserts.end(), group.ids.begin(), group.ids.end());
    }

    const std::size_t projectedCount = residentIds_.size() - pendingRemovals_.size() +
        pendingUpserts_.size() + plan.upserts.size();
    if (projectedCount <= streamingConfig_.residentPatchBudget) {
        return plan;
    }

    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash> protectedIds = [&] {
        const std::vector<PlanetPatchId> protectedVector = protectedCacheIds();
        std::unordered_set<PlanetPatchId, PlanetPatchIdHash> result{
            protectedVector.begin(),
            protectedVector.end(),
        };
        result.insert(plan.upserts.begin(), plan.upserts.end());
        return result;
    }();
    std::vector<PlanetPatchId> candidates;
    for (const PlanetPatchId& id : residentIds_) {
        if (!protectedIds.contains(id) && !pendingRemovals_.contains(id)) {
            candidates.push_back(id);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [this](const PlanetPatchId& lhs, const PlanetPatchId& rhs) {
        const std::uint64_t lhsTick = lastUsedTick_.contains(lhs) ? lastUsedTick_.at(lhs) : 0;
        const std::uint64_t rhsTick = lastUsedTick_.contains(rhs) ? lastUsedTick_.at(rhs) : 0;
        if (lhsTick != rhsTick) {
            return lhsTick < rhsTick;
        }
        return PlanetPatchIdLess{}(lhs, rhs);
    });

    const std::size_t removalCount = projectedCount - streamingConfig_.residentPatchBudget;
    if (candidates.size() < removalCount) {
        plan.upserts.clear();
        return plan;
    }
    plan.removals.assign(candidates.begin(), candidates.begin() + removalCount);
    std::sort(plan.removals.begin(), plan.removals.end(), PlanetPatchIdLess{});
    return plan;
}

void PlanetLodCoordinator::beginPatchPlan(
        const PlanetLodPatchPlan& plan,
        std::uint64_t requestRevision) {
    if (requestRevision == 0) {
        throw std::invalid_argument{"planet LOD patch request revision must be initialized"};
    }
    if (requestRevision < minimumAcceptedRevision_) {
        throw std::invalid_argument{"planet LOD patch request revision is stale"};
    }
    if (plan.upserts.size() > streamingConfig_.patchGenerationBudget) {
        throw std::invalid_argument{"planet LOD patch plan exceeds the generation budget"};
    }
    const std::size_t maximumPendingUpserts =
        streamingConfig_.patchGenerationBudget *
        streamingConfig_.maximumConcurrentPatchJobs;
    if (plan.upserts.size() > maximumPendingUpserts -
            std::min(maximumPendingUpserts, pendingUpserts_.size())) {
        throw std::invalid_argument{"planet LOD patch plan exceeds bounded pending work"};
    }
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> uniqueUpserts;
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> uniqueRemovals;
    const std::vector<PlanetPatchId> protectedVector = protectedCacheIds();
    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash> protectedIds{
        protectedVector.begin(),
        protectedVector.end(),
    };
    for (const PlanetPatchId& id : plan.upserts) {
        validate(id);
        if (residentIds_.contains(id) || pendingUpserts_.contains(id) ||
                pendingRemovals_.contains(id) || !uniqueUpserts.insert(id).second) {
            throw std::invalid_argument{"planet LOD patch plan contains a duplicate request"};
        }
    }
    for (const PlanetPatchId& id : plan.removals) {
        validate(id);
        if (!residentIds_.contains(id) || pendingUpserts_.contains(id) ||
                pendingRemovals_.contains(id) || uniqueUpserts.contains(id) ||
                protectedIds.contains(id) || !uniqueRemovals.insert(id).second) {
            throw std::invalid_argument{"planet LOD patch plan contains an invalid removal"};
        }
    }
    for (const PlanetPatchId& id : plan.upserts) {
        pendingUpserts_.emplace(id, requestRevision);
    }
    for (const PlanetPatchId& id : plan.removals) {
        pendingRemovals_.emplace(id, requestRevision);
    }
}

void PlanetLodCoordinator::cancelPendingRequests() {
    pendingUpserts_.clear();
    pendingRemovals_.clear();
}

void PlanetLodCoordinator::cancelPendingRequestsBefore(
        std::uint64_t requestRevision) {
    if (requestRevision == 0) {
        throw std::invalid_argument{"planet LOD cancellation revision must be initialized"};
    }
    minimumAcceptedRevision_ = std::max(
        minimumAcceptedRevision_,
        requestRevision);
    std::erase_if(pendingUpserts_, [requestRevision](const auto& entry) {
        return entry.second < requestRevision;
    });
    std::erase_if(pendingRemovals_, [requestRevision](const auto& entry) {
        return entry.second < requestRevision;
    });
}

void PlanetLodCoordinator::cancelPatchPlan(
        const PlanetLodPatchPlan& plan,
        std::uint64_t requestRevision) {
    const auto eraseMatching = [requestRevision](auto& pending, const PlanetPatchId& id) {
        const auto entry = pending.find(id);
        if (entry != pending.end() && entry->second == requestRevision) {
            pending.erase(entry);
        }
    };
    for (const PlanetPatchId& id : plan.upserts) {
        eraseMatching(pendingUpserts_, id);
    }
    for (const PlanetPatchId& id : plan.removals) {
        eraseMatching(pendingRemovals_, id);
    }
}

bool PlanetLodCoordinator::publishPatchChanges(
        std::span<const PlanetPatchId> upserts,
        std::span<const PlanetPatchId> removals,
        std::uint64_t requestRevision,
        std::span<const PlanetPatchErrorSample> errorSamples) {
    const auto eraseMatching = [requestRevision](auto& pending, const PlanetPatchId& id) {
        const auto entry = pending.find(id);
        if (entry != pending.end() &&
                (requestRevision == 0 || entry->second == requestRevision)) {
            pending.erase(entry);
        }
    };
    if (requestRevision != 0 && requestRevision < minimumAcceptedRevision_) {
        for (const PlanetPatchId& id : upserts) {
            eraseMatching(pendingUpserts_, id);
        }
        for (const PlanetPatchId& id : removals) {
            eraseMatching(pendingRemovals_, id);
        }
        return false;
    }
    const std::vector<PlanetPatchId> protectedVector = protectedCacheIds();
    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash> protectedIds{
        protectedVector.begin(),
        protectedVector.end(),
    };
    for (const PlanetPatchId& id : removals) {
        validate(id);
        eraseMatching(pendingRemovals_, id);
        if (!protectedIds.contains(id)) {
            residentIds_.erase(id);
            knownPatchErrors_.erase(id);
            lastUsedTick_.erase(id);
        }
    }
    for (const PlanetPatchId& id : upserts) {
        validate(id);
        residentIds_.insert(id);
        eraseMatching(pendingUpserts_, id);
        lastUsedTick_[id] = selectionTick_;
    }
    for (const PlanetPatchErrorSample& sample : errorSamples) {
        validate(sample.id);
        if (!std::isfinite(sample.maximumWorldError) ||
                sample.maximumWorldError < 0.0) {
            throw std::invalid_argument{"planet patch cached error sample is invalid"};
        }
        knownPatchErrors_[sample.id] = std::max(
            knownPatchErrors_.contains(sample.id)
                ? knownPatchErrors_.at(sample.id)
                : 0.0,
            sample.maximumWorldError);
    }
    pruneKnownPatchErrors();

    if (activeLeaves_.empty()) {
        const std::vector<PlanetPatchId> roots = rootPatchIds();
        if (std::all_of(roots.begin(), roots.end(), [this](const PlanetPatchId& id) {
                return residentIds_.contains(id);
            })) {
            activeLeaves_ = roots;
        }
    }
    reconcileActiveLeaves();
    rebuildVisibleActiveLeaves();
    return true;
}

void PlanetLodCoordinator::reconcileActiveLeaves() {
    if (activeLeaves_.empty()) {
        return;
    }

    for (PlanetLodTransitionState& transition : transitions_) {
        transition.kind = desiredIsCoarserThan(transition.parent, desiredLeaves_)
            ? PlanetLodTransitionKind::Merge
            : PlanetLodTransitionKind::Split;
    }

    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> active{
        activeLeaves_.begin(),
        activeLeaves_.end(),
    };

    std::vector<PlanetPatchId> ordered = sortedIds(active);
    std::sort(ordered.begin(), ordered.end(), [](const PlanetPatchId& lhs, const PlanetPatchId& rhs) {
        if (lhs.level != rhs.level) {
            return lhs.level > rhs.level;
        }
        return PlanetPatchIdLess{}(lhs, rhs);
    });
    for (const PlanetPatchId& leaf : ordered) {
        const std::optional<PlanetPatchId> parentId = parent(leaf);
        if (!parentId || !residentIds_.contains(*parentId) ||
                transitionLocks(*parentId)) {
            continue;
        }
        const std::array<PlanetPatchId, 4> siblings = children(*parentId);
        if (leaf != siblings.front() || !containsAll(active, siblings) ||
                !desiredIsCoarserThan(*parentId, desiredLeaves_)) {
            continue;
        }
        auto candidate = active;
        for (const PlanetPatchId& sibling : siblings) {
            candidate.erase(sibling);
        }
        candidate.insert(*parentId);
        if (leavesAreBalanced(candidate)) {
            transitions_.push_back({
                .parent = *parentId,
                .kind = PlanetLodTransitionKind::Merge,
                .childMorph = 1.0,
            });
        }
    }

    ordered = sortedIds(active);
    std::sort(ordered.begin(), ordered.end(), [](const PlanetPatchId& lhs, const PlanetPatchId& rhs) {
        if (lhs.level != rhs.level) {
            return lhs.level < rhs.level;
        }
        return PlanetPatchIdLess{}(lhs, rhs);
    });
    for (const PlanetPatchId& leaf : ordered) {
        if (!active.contains(leaf) || transitionLocks(leaf) ||
                !hasDescendant(leaf, desiredLeaves_)) {
            continue;
        }
        const std::array<PlanetPatchId, 4> childIds = children(leaf);
        if (!containsAll(residentIds_, childIds)) {
            continue;
        }
        auto candidate = active;
        candidate.erase(leaf);
        candidate.insert(childIds.begin(), childIds.end());
        if (leavesAreBalanced(candidate)) {
            active.swap(candidate);
            transitions_.push_back({
                .parent = leaf,
                .kind = PlanetLodTransitionKind::Split,
                .childMorph = 0.0,
            });
        }
    }

    std::sort(transitions_.begin(), transitions_.end(), [](const auto& lhs, const auto& rhs) {
        return PlanetPatchIdLess{}(lhs.parent, rhs.parent);
    });
    activeLeaves_ = sortedIds(active);
}

void PlanetLodCoordinator::rebuildVisibleActiveLeaves() {
    if (!latestView_) {
        visibleActiveLeaves_ = activeLeaves_;
    } else {
        visibleActiveLeaves_ = selector_.visibleLeaves(activeLeaves_, *latestView_, surface_);
    }

    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash> active{
        activeLeaves_.begin(),
        activeLeaves_.end(),
    };
    visibleDrawStates_.clear();
    visibleDrawStates_.reserve(visibleActiveLeaves_.size());
    for (const PlanetPatchId& id : visibleActiveLeaves_) {
        float morph = 1.0F;
        const std::optional<PlanetPatchId> parentId = parent(id);
        if (parentId) {
            const auto transition = std::find_if(
                transitions_.begin(),
                transitions_.end(),
                [&parentId](const PlanetLodTransitionState& value) {
                    return value.parent == *parentId;
                });
            if (transition != transitions_.end()) {
                morph = static_cast<float>(transition->childMorph);
            }
        }
        visibleDrawStates_.push_back({
            .id = id,
            .morph = morph,
            .stitchMask = stitchMaskFromActiveSet(id, active),
        });
    }
}

bool PlanetLodCoordinator::transitionLocks(const PlanetPatchId& id) const {
    return std::any_of(
        transitions_.begin(),
        transitions_.end(),
        [&id](const PlanetLodTransitionState& transition) {
            return isDescendantOrSelf(id, transition.parent) ||
                isDescendantOrSelf(transition.parent, id);
        });
}

void PlanetLodCoordinator::recordView(const PlanetLodView& view) {
    if (latestView_ && sameView(*latestView_, view)) {
        return;
    }
    previousView_ = latestView_;
    latestView_ = view;
}

void PlanetLodCoordinator::pruneKnownPatchErrors() {
    if (knownPatchErrors_.size() <= streamingConfig_.residentPatchBudget) {
        return;
    }
    const std::vector<PlanetPatchId> protectedVector = protectedCacheIds();
    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash> protectedIds{
        protectedVector.begin(),
        protectedVector.end(),
    };
    std::vector<PlanetPatchId> candidates;
    for (const auto& [id, error] : knownPatchErrors_) {
        static_cast<void>(error);
        if (!protectedIds.contains(id)) {
            candidates.push_back(id);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [this](const auto& lhs, const auto& rhs) {
        const std::uint64_t lhsTick = lastUsedTick_.contains(lhs) ? lastUsedTick_.at(lhs) : 0;
        const std::uint64_t rhsTick = lastUsedTick_.contains(rhs) ? lastUsedTick_.at(rhs) : 0;
        if (lhsTick != rhsTick) {
            return lhsTick < rhsTick;
        }
        return PlanetPatchIdLess{}(lhs, rhs);
    });
    const std::size_t removalCount = std::min(
        candidates.size(),
        knownPatchErrors_.size() - streamingConfig_.residentPatchBudget);
    for (std::size_t index = 0; index < removalCount; ++index) {
        knownPatchErrors_.erase(candidates[index]);
    }
}

std::vector<PlanetPatchId> PlanetLodCoordinator::protectedCacheIds() const {
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> result{
        activeLeaves_.begin(),
        activeLeaves_.end(),
    };
    for (const auto& [id, revision] : pendingUpserts_) {
        static_cast<void>(revision);
        result.insert(id);
    }
    for (const PlanetLodTransitionState& transition : transitions_) {
        result.insert(transition.parent);
        const std::array<PlanetPatchId, 4> childIds = children(transition.parent);
        result.insert(childIds.begin(), childIds.end());
    }

    for (const PlanetPatchId& leaf : activeLeaves_) {
        if (hasDescendant(leaf, desiredLeaves_)) {
            for (const PlanetPatchId& childId : children(leaf)) {
                if (residentIds_.contains(childId)) {
                    result.insert(childId);
                }
            }
        }
        const std::optional<PlanetPatchId> parentId = parent(leaf);
        const bool desiredCoarser = parentId && std::any_of(
            desiredLeaves_.begin(),
            desiredLeaves_.end(),
            [&parentId](const PlanetPatchId& desired) {
                return isDescendantOrSelf(*parentId, desired);
            });
        if (parentId && desiredCoarser && residentIds_.contains(*parentId)) {
            result.insert(*parentId);
        }
    }
    return sortedIds(result);
}

} // namespace wgen
