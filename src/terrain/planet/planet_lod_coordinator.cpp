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
            streamingConfig_.patchUploadBudget < 4 ||
            streamingConfig_.patchUploadBudget > streamingConfig_.residentPatchBudget) {
        throw std::invalid_argument{"planet LOD streaming budgets are invalid"};
    }
    if (lodConfig_.selectedLeafBudget + streamingConfig_.patchUploadBudget >
            streamingConfig_.residentPatchBudget) {
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
    pendingIds_.clear();
    lastUsedTick_.clear();
    latestView_.reset();
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
    latestView_ = view;
    ++selectionTick_;
    const PlanetLodSelection selection = selector_.select(
        view,
        surface_,
        lodConfig_,
        desiredLeaves_);
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
    latestView_ = view;
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
    if (!pendingIds_.empty()) {
        return {};
    }

    struct RequestGroup {
        bool merge{};
        std::uint8_t level{};
        PlanetPatchId anchor{};
        std::vector<PlanetPatchId> ids{};
    };
    std::vector<RequestGroup> groups;
    const std::unordered_set<PlanetPatchId, PlanetPatchIdHash> active{
        activeLeaves_.begin(),
        activeLeaves_.end(),
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
        if (desiredCoarser && !residentIds_.contains(*parentId)) {
            groups.push_back({
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
            if (!residentIds_.contains(childId)) {
                missing.push_back(childId);
            }
        }
        if (!missing.empty()) {
            groups.push_back({
                .merge = false,
                .level = leaf.level,
                .anchor = leaf,
                .ids = std::move(missing),
            });
        }
    }

    std::sort(groups.begin(), groups.end(), [](const RequestGroup& lhs, const RequestGroup& rhs) {
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
        if (plan.upserts.size() + group.ids.size() > streamingConfig_.patchUploadBudget) {
            continue;
        }
        plan.upserts.insert(plan.upserts.end(), group.ids.begin(), group.ids.end());
    }
    std::sort(plan.upserts.begin(), plan.upserts.end(), PlanetPatchIdLess{});
    plan.upserts.erase(std::unique(plan.upserts.begin(), plan.upserts.end()), plan.upserts.end());

    const std::size_t projectedCount = residentIds_.size() + plan.upserts.size();
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
        if (!protectedIds.contains(id)) {
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

void PlanetLodCoordinator::beginPatchPlan(const PlanetLodPatchPlan& plan) {
    if (!pendingIds_.empty()) {
        throw std::logic_error{"planet LOD patch requests are already pending"};
    }
    if (plan.upserts.size() > streamingConfig_.patchUploadBudget) {
        throw std::invalid_argument{"planet LOD patch plan exceeds the upload budget"};
    }
    for (const PlanetPatchId& id : plan.upserts) {
        validate(id);
        if (residentIds_.contains(id) || !pendingIds_.insert(id).second) {
            throw std::invalid_argument{"planet LOD patch plan contains a duplicate request"};
        }
    }
    for (const PlanetPatchId& id : plan.removals) {
        validate(id);
        if (!residentIds_.contains(id) || pendingIds_.contains(id)) {
            pendingIds_.clear();
            throw std::invalid_argument{"planet LOD patch plan contains an invalid removal"};
        }
    }
}

void PlanetLodCoordinator::cancelPendingRequests() {
    pendingIds_.clear();
}

void PlanetLodCoordinator::publishPatchChanges(
        std::span<const PlanetPatchId> upserts,
        std::span<const PlanetPatchId> removals) {
    for (const PlanetPatchId& id : removals) {
        residentIds_.erase(id);
        lastUsedTick_.erase(id);
    }
    for (const PlanetPatchId& id : upserts) {
        validate(id);
        residentIds_.insert(id);
        pendingIds_.erase(id);
        lastUsedTick_[id] = selectionTick_;
    }
    pendingIds_.clear();

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

std::vector<PlanetPatchId> PlanetLodCoordinator::protectedCacheIds() const {
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> result{
        activeLeaves_.begin(),
        activeLeaves_.end(),
    };
    result.insert(pendingIds_.begin(), pendingIds_.end());
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
