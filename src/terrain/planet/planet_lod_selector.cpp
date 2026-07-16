#include "planet_lod_selector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace wgen {
namespace {

constexpr std::array PLANET_PATCH_EDGES{
    PlanetPatchEdge::UMin,
    PlanetPatchEdge::UMax,
    PlanetPatchEdge::VMin,
    PlanetPatchEdge::VMax,
};

bool isFinite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct PatchGeometry {
    glm::dvec3 direction{};
    double angularRadius{};
    glm::dvec3 center{};
    double boundingRadius{};
};

glm::dvec3 patchDirection(CubeSphereFace face, double u, double v) {
    return cubeSphereDirection(face, u, v);
}

PatchGeometry patchGeometry(const PlanetPatchId& id, const PlanetLodSurface& surface) {
    const PlanetPatchBounds bounds = patchBounds(id);
    const double centerU = std::midpoint(bounds.uMin, bounds.uMax);
    const double centerV = std::midpoint(bounds.vMin, bounds.vMax);
    const glm::dvec3 direction = glm::normalize(patchDirection(id.face, centerU, centerV));
    const std::array corners{
        patchDirection(id.face, bounds.uMin, bounds.vMin),
        patchDirection(id.face, bounds.uMax, bounds.vMin),
        patchDirection(id.face, bounds.uMin, bounds.vMax),
        patchDirection(id.face, bounds.uMax, bounds.vMax),
    };

    double angularRadius = 0.0;
    for (const glm::dvec3& corner : corners) {
        const double cosine = std::clamp(glm::dot(direction, glm::normalize(corner)), -1.0, 1.0);
        angularRadius = std::max(angularRadius, std::acos(cosine));
    }

    const double outerRadius = surface.radius + surface.maximumDisplacement;
    const double maximumAbsoluteDisplacement = std::max(
        std::abs(surface.minimumDisplacement),
        std::abs(surface.maximumDisplacement));
    return {
        .direction = direction,
        .angularRadius = angularRadius,
        .center = direction * surface.radius,
        .boundingRadius = 2.0 * outerRadius * std::sin(angularRadius / 2.0) +
            maximumAbsoluteDisplacement,
    };
}

bool isDescendantOrSelf(const PlanetPatchId& id, const PlanetPatchId& ancestor) {
    if (id.face != ancestor.face || id.level < ancestor.level) {
        return false;
    }
    const std::uint8_t difference = static_cast<std::uint8_t>(id.level - ancestor.level);
    return (id.x >> difference) == ancestor.x && (id.y >> difference) == ancestor.y;
}

PlanetPatchId ancestorAtLevel(const PlanetPatchId& id, std::uint8_t level) {
    if (level > id.level) {
        throw std::invalid_argument{"planet patch ancestor level is finer than the patch"};
    }
    const std::uint8_t difference = static_cast<std::uint8_t>(id.level - level);
    return {id.face, level, id.x >> difference, id.y >> difference};
}

std::optional<PlanetPatchId> selectedCoveringAncestor(
        const PlanetPatchId& id,
        const std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& leaves) {
    PlanetPatchId current = id;
    while (true) {
        if (leaves.contains(current)) {
            return current;
        }
        const std::optional<PlanetPatchId> next = parent(current);
        if (!next) {
            return std::nullopt;
        }
        current = *next;
    }
}

void balanceLeaves(std::unordered_set<PlanetPatchId, PlanetPatchIdHash>& leaves) {
    while (true) {
        std::vector<PlanetPatchId> ordered{leaves.begin(), leaves.end()};
        std::sort(ordered.begin(), ordered.end(), [](const PlanetPatchId& lhs, const PlanetPatchId& rhs) {
            if (lhs.level != rhs.level) {
                return lhs.level > rhs.level;
            }
            return PlanetPatchIdLess{}(lhs, rhs);
        });

        std::optional<PlanetPatchId> coarsenTarget;
        for (const PlanetPatchId& leaf : ordered) {
            for (const PlanetPatchEdge edge : PLANET_PATCH_EDGES) {
                const PlanetPatchId sameLevel = sameLevelNeighbor(leaf, edge).id;
                const std::optional<PlanetPatchId> neighbor = selectedCoveringAncestor(sameLevel, leaves);
                if (neighbor && neighbor->level + 1 < leaf.level) {
                    coarsenTarget = ancestorAtLevel(
                        leaf,
                        static_cast<std::uint8_t>(neighbor->level + 1));
                    break;
                }
            }
            if (coarsenTarget) {
                break;
            }
        }

        if (!coarsenTarget) {
            return;
        }

        std::erase_if(leaves, [&coarsenTarget](const PlanetPatchId& leaf) {
            return isDescendantOrSelf(leaf, *coarsenTarget);
        });
        leaves.insert(*coarsenTarget);
    }
}

struct SplitCandidate {
    PlanetPatchId id{};
    double priority{};
};

struct SplitCandidateLess {
    bool operator()(const SplitCandidate& lhs, const SplitCandidate& rhs) const {
        if (lhs.priority != rhs.priority) {
            return lhs.priority < rhs.priority;
        }
        return PlanetPatchIdLess{}(rhs.id, lhs.id);
    }
};

std::size_t minimumLeafCount(std::uint8_t minimumLevel) {
    const std::uint64_t count = patchesPerAxis(minimumLevel);
    const std::uint64_t result = FACES.size() * count * count;
    if (result > std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error{"minimum planet LOD leaf count overflows size_t"};
    }
    return static_cast<std::size_t>(result);
}

} // namespace

void validatePlanetLodConfig(const PlanetLodConfig& config) {
    if (config.minimumLevel > config.maximumLevel ||
            config.maximumLevel > MAX_PLANET_PATCH_LEVEL) {
        throw std::invalid_argument{"planet LOD level range is invalid"};
    }
    if (!std::isfinite(config.targetErrorPixels) || config.targetErrorPixels <= 0.0) {
        throw std::invalid_argument{"planet LOD target error must be finite and positive"};
    }
    if (!std::isfinite(config.mergeRatio) || config.mergeRatio <= 0.0 || config.mergeRatio >= 1.0) {
        throw std::invalid_argument{"planet LOD merge ratio must be between zero and one"};
    }
    if (config.patchQuadCount == 0) {
        throw std::invalid_argument{"planet LOD patch quad count must be positive"};
    }
    if (config.selectedLeafBudget < minimumLeafCount(config.minimumLevel)) {
        throw std::invalid_argument{"planet LOD leaf budget cannot cover its minimum level"};
    }
}

PlanetLodErrorThresholds planetLodErrorThresholds(
        const PlanetLodConfig& config) {
    validatePlanetLodConfig(config);
    return {
        .splitPixels = config.targetErrorPixels,
        .mergePixels = config.targetErrorPixels * config.mergeRatio,
    };
}

void validatePlanetLodView(const PlanetLodView& view) {
    if (!isFinite(view.position) || !isFinite(view.forward) ||
            !isFinite(view.right) || !isFinite(view.up)) {
        throw std::invalid_argument{"planet LOD view vectors must be finite"};
    }
    constexpr double UNIT_EPSILON = 0.0001;
    if (std::abs(glm::length(view.forward) - 1.0) > UNIT_EPSILON ||
            std::abs(glm::length(view.right) - 1.0) > UNIT_EPSILON ||
            std::abs(glm::length(view.up) - 1.0) > UNIT_EPSILON ||
            std::abs(glm::dot(view.forward, view.right)) > UNIT_EPSILON ||
            std::abs(glm::dot(view.forward, view.up)) > UNIT_EPSILON ||
            std::abs(glm::dot(view.right, view.up)) > UNIT_EPSILON) {
        throw std::invalid_argument{"planet LOD view basis must be orthonormal"};
    }
    if (!std::isfinite(view.verticalFov) || view.verticalFov <= 0.0 ||
            view.verticalFov >= std::numbers::pi) {
        throw std::invalid_argument{"planet LOD vertical FOV is invalid"};
    }
    if (!std::isfinite(view.aspectRatio) || view.aspectRatio <= 0.0 ||
            !std::isfinite(view.nearPlane) || view.nearPlane <= 0.0 ||
            !std::isfinite(view.farPlane) || view.farPlane <= view.nearPlane ||
            view.viewportHeight == 0) {
        throw std::invalid_argument{"planet LOD projection parameters are invalid"};
    }
}

void validatePlanetLodSurface(const PlanetLodSurface& surface) {
    if (!std::isfinite(surface.radius) || surface.radius <= 0.0 ||
            !std::isfinite(surface.minimumDisplacement) ||
            !std::isfinite(surface.maximumDisplacement) ||
            surface.minimumDisplacement > 0.0 ||
            surface.maximumDisplacement < 0.0 ||
            surface.minimumDisplacement > surface.maximumDisplacement ||
            surface.radius + surface.maximumDisplacement <= 0.0) {
        throw std::invalid_argument{"planet LOD surface bounds are invalid"};
    }
    for (const double error : surface.maximumOmittedDetailError) {
        if (!std::isfinite(error) || error < 0.0) {
            throw std::invalid_argument{"planet LOD omitted-detail bounds are invalid"};
        }
    }
}

double PlanetPatchWorldError::maximum() const {
    return std::max({sphereCurvature, omittedTerrainDetail, cachedData});
}

PlanetPatchWorldError planetPatchWorldError(
        const PlanetPatchId& id,
        const PlanetLodSurface& surface,
        std::uint32_t patchQuadCount,
        double knownCachedDataError) {
    validate(id);
    validatePlanetLodSurface(surface);
    if (patchQuadCount == 0) {
        throw std::invalid_argument{"planet LOD patch quad count must be positive"};
    }
    if (!std::isfinite(knownCachedDataError) || knownCachedDataError < 0.0) {
        throw std::invalid_argument{"planet LOD cached data error must be finite and non-negative"};
    }

    const PatchGeometry geometry = patchGeometry(id, surface);
    const double cellAngle = 2.0 * geometry.angularRadius /
        static_cast<double>(patchQuadCount);
    const double outerRadius = surface.radius + surface.maximumDisplacement;
    return {
        .sphereCurvature = outerRadius * (1.0 - std::cos(cellAngle * 0.5)),
        .omittedTerrainDetail = surface.maximumOmittedDetailError[id.level],
        .cachedData = knownCachedDataError,
    };
}

double planetPatchScreenErrorPixels(
        const PlanetPatchId& id,
        const PlanetLodView& view,
        const PlanetLodSurface& surface,
        std::uint32_t patchQuadCount,
    double knownCachedDataError) {
    validatePlanetLodView(view);
    const double worldError = planetPatchWorldError(
        id,
        surface,
        patchQuadCount,
        knownCachedDataError).maximum();
    const PatchGeometry geometry = patchGeometry(id, surface);
    const double distance = std::max(
        glm::length(view.position - geometry.center) - geometry.boundingRadius,
        0.000001);
    return worldError * static_cast<double>(view.viewportHeight) /
        (2.0 * distance * std::tan(view.verticalFov / 2.0));
}

bool isPlanetPatchVisible(
        const PlanetPatchId& id,
        const PlanetLodView& view,
        const PlanetLodSurface& surface) {
    validate(id);
    validatePlanetLodView(view);
    validatePlanetLodSurface(surface);

    const PatchGeometry geometry = patchGeometry(id, surface);
    const glm::dvec3 offset = geometry.center - view.position;
    const double depth = glm::dot(offset, view.forward);
    if (depth + geometry.boundingRadius < view.nearPlane ||
            depth - geometry.boundingRadius > view.farPlane) {
        return false;
    }

    const double verticalTangent = std::tan(view.verticalFov / 2.0);
    const double horizontalTangent = verticalTangent * view.aspectRatio;
    const double projectedDepth = std::max(depth, view.nearPlane);
    const double verticalLimit = projectedDepth * verticalTangent +
        geometry.boundingRadius * std::sqrt(1.0 + verticalTangent * verticalTangent);
    const double horizontalLimit = projectedDepth * horizontalTangent +
        geometry.boundingRadius * std::sqrt(1.0 + horizontalTangent * horizontalTangent);
    if (std::abs(glm::dot(offset, view.up)) > verticalLimit ||
            std::abs(glm::dot(offset, view.right)) > horizontalLimit) {
        return false;
    }

    const double cameraDistance = glm::length(view.position);
    const double outerRadius = surface.radius + surface.maximumDisplacement;
    if (cameraDistance <= outerRadius) {
        return true;
    }

    const double innerRadius = std::max(
        surface.radius + surface.minimumDisplacement,
        0.0);
    if (innerRadius == 0.0) {
        return true;
    }

    const double centerAngle = std::acos(std::clamp(
        glm::dot(glm::normalize(view.position), geometry.direction),
        -1.0,
        1.0));
    const double horizonAngle =
        std::acos(std::clamp(innerRadius / cameraDistance, 0.0, 1.0)) +
        std::acos(std::clamp(innerRadius / outerRadius, 0.0, 1.0));
    return centerAngle <= horizonAngle + geometry.angularRadius;
}

PlanetLodSelection PlanetLodSelector::select(
        const PlanetLodView& view,
        const PlanetLodSurface& surface,
        const PlanetLodConfig& config,
        std::span<const PlanetPatchId> previousLeaves,
        const PlanetPatchErrorCache& cachedErrors) const {
    validatePlanetLodConfig(config);
    validatePlanetLodView(view);
    validatePlanetLodSurface(surface);
    const PlanetLodErrorThresholds thresholds =
        planetLodErrorThresholds(config);

    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> previousSplitNodes;
    for (const PlanetPatchId& leaf : previousLeaves) {
        validate(leaf);
        std::optional<PlanetPatchId> ancestor = parent(leaf);
        while (ancestor) {
            previousSplitNodes.insert(*ancestor);
            ancestor = parent(*ancestor);
        }
    }

    std::unordered_set<PlanetPatchId, PlanetPatchIdHash> leaves;
    std::priority_queue<SplitCandidate, std::vector<SplitCandidate>, SplitCandidateLess> candidates;
    const auto addCandidate = [&](const PlanetPatchId& id, auto& queue) {
        if (id.level >= config.maximumLevel ||
                (id.level >= config.minimumLevel && !isPlanetPatchVisible(id, view, surface))) {
            return;
        }

        const bool wasSplit = previousSplitNodes.contains(id);
        const double threshold = wasSplit
            ? thresholds.mergePixels
            : thresholds.splitPixels;
        const double error = planetPatchScreenErrorPixels(
            id,
            view,
            surface,
            config.patchQuadCount,
            cachedErrors.contains(id) ? cachedErrors.at(id) : 0.0);
        const bool forced = id.level < config.minimumLevel;
        if (forced || (wasSplit ? error >= threshold : error > threshold)) {
            queue.push({
                .id = id,
                .priority = forced ? std::numeric_limits<double>::infinity() : error / threshold,
            });
        }
    };

    for (const CubeSphereFace face : FACES) {
        const PlanetPatchId root{face, 0, 0, 0};
        leaves.insert(root);
        addCandidate(root, candidates);
    }

    while (!candidates.empty() && leaves.size() + 3 <= config.selectedLeafBudget) {
        const SplitCandidate candidate = candidates.top();
        candidates.pop();
        if (!leaves.erase(candidate.id)) {
            continue;
        }

        for (const PlanetPatchId& childId : children(candidate.id)) {
            leaves.insert(childId);
            addCandidate(childId, candidates);
        }
    }

    balanceLeaves(leaves);

    PlanetLodSelection result;
    result.leaves.assign(leaves.begin(), leaves.end());
    std::sort(result.leaves.begin(), result.leaves.end(), PlanetPatchIdLess{});
    result.visibleLeaves = visibleLeaves(result.leaves, view, surface);
    return result;
}

std::vector<PlanetPatchId> PlanetLodSelector::visibleLeaves(
        std::span<const PlanetPatchId> leaves,
        const PlanetLodView& view,
        const PlanetLodSurface& surface) const {
    validatePlanetLodView(view);
    validatePlanetLodSurface(surface);

    std::vector<PlanetPatchId> result;
    result.reserve(leaves.size());
    for (const PlanetPatchId& id : leaves) {
        if (isPlanetPatchVisible(id, view, surface)) {
            result.push_back(id);
        }
    }
    std::sort(result.begin(), result.end(), PlanetPatchIdLess{});
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

} // namespace wgen
