#include "terrain/planet/local_clipmap_controller.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>

namespace wgen {
namespace {

bool isFinite(const glm::dvec2& value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

double angularDistance(glm::dvec3 lhs, glm::dvec3 rhs) {
    return std::acos(std::clamp(
        glm::dot(glm::normalize(lhs), glm::normalize(rhs)),
        -1.0,
        1.0));
}

} // namespace

void validateLocalClipmapControllerConfig(
        const LocalClipmapControllerConfig& config) {
    if (!std::isfinite(config.activationAltitudeMeters) ||
            !std::isfinite(config.deactivationAltitudeMeters) ||
            config.activationAltitudeMeters <= 0.0 ||
            config.deactivationAltitudeMeters <=
                config.activationAltitudeMeters ||
            !std::isfinite(config.activationGroundResolutionMeters) ||
            !std::isfinite(config.deactivationGroundResolutionMeters) ||
            config.activationGroundResolutionMeters <= 0.0 ||
            config.deactivationGroundResolutionMeters <=
                config.activationGroundResolutionMeters ||
            !std::isfinite(config.coolingDurationSeconds) ||
            config.coolingDurationSeconds < 0.0) {
        throw std::invalid_argument{
            "local clipmap controller config is invalid"};
    }
}

void validateLocalClipmapControllerView(
        const LocalClipmapControllerView& view) {
    if (!std::isfinite(view.altitudeMeters) || view.altitudeMeters < 0.0 ||
            !std::isfinite(view.requestedGroundResolutionMeters) ||
            view.requestedGroundResolutionMeters <= 0.0) {
        throw std::invalid_argument{
            "local clipmap controller view is invalid"};
    }
}

void validateLocalClipmapFootprint(
        const LocalClipmapFootprint& footprint) {
    validateLocalPlanetFrame(footprint.frame);
    if (!isFinite(footprint.centerMeters) ||
            !std::isfinite(footprint.innerHalfExtentMeters) ||
            !std::isfinite(footprint.outerHalfExtentMeters) ||
            footprint.innerHalfExtentMeters <= 0.0 ||
            footprint.outerHalfExtentMeters <=
                footprint.innerHalfExtentMeters ||
            footprint.terrainEpoch == 0) {
        throw std::invalid_argument{"local clipmap footprint is invalid"};
    }
}

double localClipmapRequestedGroundResolutionMeters(
        double altitudeMeters,
        double verticalFovRadians,
        std::uint32_t viewportHeightPixels) {
    if (!std::isfinite(altitudeMeters) || altitudeMeters < 0.0 ||
            !std::isfinite(verticalFovRadians) ||
            verticalFovRadians <= 0.0 ||
            verticalFovRadians >= std::numbers::pi ||
            viewportHeightPixels == 0) {
        throw std::invalid_argument{
            "local clipmap ground-resolution view is invalid"};
    }
    return std::max(
        1.0e-9,
        2.0 * altitudeMeters * std::tan(verticalFovRadians / 2.0) /
            static_cast<double>(viewportHeightPixels));
}

LocalClipmapController::LocalClipmapController(
        LocalClipmapControllerConfig config)
    : config_{config} {
    validateLocalClipmapControllerConfig(config_);
}

void LocalClipmapController::update(
        const LocalClipmapControllerView& view,
        bool completeCurrentCoverage,
        double deltaSeconds) {
    validateLocalClipmapControllerView(view);
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        throw std::invalid_argument{
            "local clipmap controller delta must be finite and non-negative"};
    }

    const bool activate = activationRequested(view);
    const bool retain = retentionRequested(view);
    switch (state_) {
        case LocalClipmapControllerState::Inactive:
            if (activate) {
                state_ = completeCurrentCoverage
                    ? LocalClipmapControllerState::Active
                    : LocalClipmapControllerState::Warming;
            }
            break;
        case LocalClipmapControllerState::Warming:
            if (!retain) {
                state_ = LocalClipmapControllerState::Inactive;
            } else if (completeCurrentCoverage) {
                state_ = LocalClipmapControllerState::Active;
            }
            break;
        case LocalClipmapControllerState::Active:
            if (!retain) {
                state_ = LocalClipmapControllerState::Cooling;
                coolingElapsedSeconds_ = 0.0;
            } else if (!completeCurrentCoverage) {
                // Global coverage is restored immediately; the old local
                // cache may stay resident but no longer owns any pixels.
                state_ = LocalClipmapControllerState::Warming;
            }
            break;
        case LocalClipmapControllerState::Cooling:
            if (activate) {
                state_ = completeCurrentCoverage
                    ? LocalClipmapControllerState::Active
                    : LocalClipmapControllerState::Warming;
                coolingElapsedSeconds_ = 0.0;
            } else {
                coolingElapsedSeconds_ += deltaSeconds;
                if (coolingElapsedSeconds_ >= config_.coolingDurationSeconds) {
                    state_ = LocalClipmapControllerState::Inactive;
                    coolingElapsedSeconds_ = 0.0;
                }
            }
            break;
    }
}

void LocalClipmapController::reset() {
    state_ = LocalClipmapControllerState::Inactive;
    coolingElapsedSeconds_ = 0.0;
}

bool LocalClipmapController::activationRequested(
        const LocalClipmapControllerView& view) const {
    return view.altitudeMeters <= config_.activationAltitudeMeters &&
        view.requestedGroundResolutionMeters <=
            config_.activationGroundResolutionMeters;
}

bool LocalClipmapController::retentionRequested(
        const LocalClipmapControllerView& view) const {
    return view.altitudeMeters < config_.deactivationAltitudeMeters &&
        view.requestedGroundResolutionMeters <
            config_.deactivationGroundResolutionMeters;
}

double localClipmapCoverage(
        const LocalClipmapFootprint& footprint,
        const glm::dvec2& localPositionMeters) {
    validateLocalClipmapFootprint(footprint);
    if (!isFinite(localPositionMeters)) {
        throw std::invalid_argument{
            "local clipmap coverage position is invalid"};
    }
    const glm::dvec2 relative =
        glm::abs(localPositionMeters - footprint.centerMeters);
    const double distance = std::max(relative.x, relative.y);
    if (distance <= footprint.innerHalfExtentMeters) {
        return 1.0;
    }
    if (distance >= footprint.outerHalfExtentMeters) {
        return 0.0;
    }
    const double linear =
        (distance - footprint.innerHalfExtentMeters) /
        (footprint.outerHalfExtentMeters -
            footprint.innerHalfExtentMeters);
    const double smooth = linear * linear * (3.0 - 2.0 * linear);
    return 1.0 - smooth;
}

double localClipmapDitherThreshold(
        std::uint32_t pixelX,
        std::uint32_t pixelY) noexcept {
    constexpr std::array<std::uint8_t, 16> BAYER_4X4{
         0,  8,  2, 10,
        12,  4, 14,  6,
         3, 11,  1,  9,
        15,  7, 13,  5,
    };
    const std::size_t index =
        (pixelY & 3U) * 4U + (pixelX & 3U);
    return (static_cast<double>(BAYER_4X4[index]) + 0.5) / 16.0;
}

bool localClipmapDitherKeepsLocal(
        double coverage,
        std::uint32_t pixelX,
        std::uint32_t pixelY) {
    if (!std::isfinite(coverage) || coverage < 0.0 || coverage > 1.0) {
        throw std::invalid_argument{
            "local clipmap dither coverage must be between zero and one"};
    }
    return localClipmapDitherThreshold(pixelX, pixelY) < coverage;
}

bool localClipmapDitherKeepsGlobal(
        double coverage,
        std::uint32_t pixelX,
        std::uint32_t pixelY) {
    return !localClipmapDitherKeepsLocal(
        coverage,
        pixelX,
        pixelY);
}

bool planetPatchFullyInsideLocalClipmap(
        const PlanetPatchId& id,
        const LocalClipmapFootprint& footprint) {
    validate(id);
    validateLocalClipmapFootprint(footprint);
    const PlanetPatchBounds bounds = patchBounds(id);
    const double centerU = std::midpoint(bounds.uMin, bounds.uMax);
    const double centerV = std::midpoint(bounds.vMin, bounds.vMax);
    const glm::dvec3 centerDirection = cubeSphereDirection(
        id.face,
        centerU,
        centerV);
    const std::array corners{
        cubeSphereDirection(id.face, bounds.uMin, bounds.vMin),
        cubeSphereDirection(id.face, bounds.uMax, bounds.vMin),
        cubeSphereDirection(id.face, bounds.uMin, bounds.vMax),
        cubeSphereDirection(id.face, bounds.uMax, bounds.vMax),
    };
    double angularRadius = 0.0;
    for (const glm::dvec3& corner : corners) {
        angularRadius = std::max(
            angularRadius,
            angularDistance(centerDirection, corner));
    }

    const double anchorAngle = angularDistance(
        footprint.frame.anchorDirection,
        centerDirection);
    const double maximumAngle = anchorAngle + angularRadius;
    if (!std::isfinite(maximumAngle) ||
            maximumAngle >= std::numbers::pi / 2.0) {
        return false;
    }
    const glm::dvec2 center = localPlanetTangentPosition(
        footprint.frame,
        centerDirection);
    if (!isFinite(center)) {
        return false;
    }
    const double expansion = maximumAngle > 1.0e-12
        ? maximumAngle / std::sin(maximumAngle)
        : 1.0;
    const double conservativeRadius =
        angularRadius * footprint.frame.planetRadiusMeters * expansion;
    if (!std::isfinite(conservativeRadius)) {
        return false;
    }
    const glm::dvec2 relative = glm::abs(center - footprint.centerMeters);
    return std::max(relative.x, relative.y) + conservativeRadius <=
        footprint.innerHalfExtentMeters;
}

bool localClipmapOwnsPlanetPatch(
        const PlanetPatchId& id,
        const LocalClipmapFootprint& footprint,
        bool allRequiredLevelsCurrent,
        std::uint64_t activeTerrainEpoch) {
    validate(id);
    validateLocalClipmapFootprint(footprint);
    return allRequiredLevelsCurrent && activeTerrainEpoch != 0 &&
        footprint.terrainEpoch == activeTerrainEpoch &&
        planetPatchFullyInsideLocalClipmap(id, footprint);
}

} // namespace wgen
