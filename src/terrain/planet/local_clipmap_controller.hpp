#pragma once

#include "terrain/planet/local_planet_frame.hpp"
#include "terrain/planet/planet_patch.hpp"

#include <cstdint>

#include <glm/glm.hpp>

namespace wgen {

enum class LocalClipmapControllerState : std::uint8_t {
    Inactive,
    Warming,
    Active,
    Cooling,
};

struct LocalClipmapControllerConfig {
    double activationAltitudeMeters{};
    double deactivationAltitudeMeters{};
    double activationGroundResolutionMeters{};
    double deactivationGroundResolutionMeters{};
    double coolingDurationSeconds{0.25};
};

struct LocalClipmapControllerView {
    double altitudeMeters{};
    double requestedGroundResolutionMeters{};
};

struct LocalClipmapFootprint {
    LocalPlanetFrame frame{};
    glm::dvec2 centerMeters{};
    double innerHalfExtentMeters{};
    double outerHalfExtentMeters{};
    std::uint64_t terrainEpoch{};
};

void validateLocalClipmapControllerConfig(
    const LocalClipmapControllerConfig& config);
void validateLocalClipmapControllerView(
    const LocalClipmapControllerView& view);
void validateLocalClipmapFootprint(const LocalClipmapFootprint& footprint);

double localClipmapRequestedGroundResolutionMeters(
    double altitudeMeters,
    double verticalFovRadians,
    std::uint32_t viewportHeightPixels);

class LocalClipmapController {
public:
    explicit LocalClipmapController(LocalClipmapControllerConfig config);

    void update(
        const LocalClipmapControllerView& view,
        bool completeCurrentCoverage,
        double deltaSeconds);
    void reset();

    LocalClipmapControllerState state() const { return state_; }
    bool needsResidency() const {
        return state_ == LocalClipmapControllerState::Warming ||
            state_ == LocalClipmapControllerState::Active;
    }
    bool ownsCoverage() const {
        return state_ == LocalClipmapControllerState::Active;
    }
    const LocalClipmapControllerConfig& config() const { return config_; }

private:
    bool activationRequested(const LocalClipmapControllerView& view) const;
    bool retentionRequested(const LocalClipmapControllerView& view) const;

    LocalClipmapControllerConfig config_{};
    LocalClipmapControllerState state_{LocalClipmapControllerState::Inactive};
    double coolingElapsedSeconds_{};
};

// One analytic square-footprint coverage function is used by CPU policy and
// mirrored in shaders/include/hybrid_coverage.glsl. It is one inside the
// opaque footprint, smoothly falls through the overlap band, and is zero at
// and beyond the outer footprint.
double localClipmapCoverage(
    const LocalClipmapFootprint& footprint,
    const glm::dvec2& localPositionMeters);

double localClipmapDitherThreshold(
    std::uint32_t pixelX,
    std::uint32_t pixelY) noexcept;
bool localClipmapDitherKeepsLocal(
    double coverage,
    std::uint32_t pixelX,
    std::uint32_t pixelY);
bool localClipmapDitherKeepsGlobal(
    double coverage,
    std::uint32_t pixelX,
    std::uint32_t pixelY);

// Conservative draw-time ownership test. False retains the global patch.
bool planetPatchFullyInsideLocalClipmap(
    const PlanetPatchId& id,
    const LocalClipmapFootprint& footprint);

bool localClipmapOwnsPlanetPatch(
    const PlanetPatchId& id,
    const LocalClipmapFootprint& footprint,
    bool allRequiredLevelsCurrent,
    std::uint64_t activeTerrainEpoch);

} // namespace wgen
