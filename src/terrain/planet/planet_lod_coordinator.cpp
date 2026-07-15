#include "planet_lod_coordinator.hpp"

#include <algorithm>
#include <array>
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

} // namespace

PlanetLodCoordinator::PlanetLodCoordinator(
        PlanetLodConfig lodConfig,
        PlanetLodStreamingConfig streamingConfig)
    : lodConfig_{lodConfig}, streamingConfig_{streamingConfig} {
    validatePlanetLodConfig(lodConfig_);
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

    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> active{
        activeLeaves_.begin(),
        activeLeaves_.end(),
    };
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<PlanetPatchId> ordered = sortedIds(active);
        std::sort(ordered.begin(), ordered.end(), [](const PlanetPatchId& lhs, const PlanetPatchId& rhs) {
            if (lhs.level != rhs.level) {
                return lhs.level > rhs.level;
            }
            return PlanetPatchIdLess{}(lhs, rhs);
        });

        for (const PlanetPatchId& leaf : ordered) {
            const std::optional<PlanetPatchId> parentId = parent(leaf);
            if (!parentId || !residentIds_.contains(*parentId)) {
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
            if (!desiredCoarser) {
                continue;
            }

            auto candidate = active;
            for (const PlanetPatchId& sibling : siblings) {
                candidate.erase(sibling);
            }
            candidate.insert(*parentId);
            if (leavesAreBalanced(candidate)) {
                active.swap(candidate);
                changed = true;
                break;
            }
        }
        if (changed) {
            continue;
        }

        ordered = sortedIds(active);
        std::sort(ordered.begin(), ordered.end(), [](const PlanetPatchId& lhs, const PlanetPatchId& rhs) {
            if (lhs.level != rhs.level) {
                return lhs.level < rhs.level;
            }
            return PlanetPatchIdLess{}(lhs, rhs);
        });
        for (const PlanetPatchId& leaf : ordered) {
            if (!hasDescendant(leaf, desiredLeaves_)) {
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
                changed = true;
                break;
            }
        }
    }

    activeLeaves_ = sortedIds(active);
}

void PlanetLodCoordinator::rebuildVisibleActiveLeaves() {
    if (!latestView_) {
        visibleActiveLeaves_ = activeLeaves_;
        return;
    }
    visibleActiveLeaves_ = selector_.visibleLeaves(activeLeaves_, *latestView_, surface_);
}

std::vector<PlanetPatchId> PlanetLodCoordinator::protectedCacheIds() const {
    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> result{
        activeLeaves_.begin(),
        activeLeaves_.end(),
    };
    result.insert(pendingIds_.begin(), pendingIds_.end());

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
