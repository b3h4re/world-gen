#pragma once

#include "terrain/planet/local_clipmap_controller.hpp"

#include <optional>

#include <glm/glm.hpp>

namespace wgen {

// Presentation contract:
// - terrain directions, heights, and simulation slope queries stay spherical;
// - visual normals are derived from the presented vertex positions;
// - a future local shadow pass must use the same presented positions;
// - atmosphere and distant/global terrain stay in spherical coordinates.
// The current renderer has no terrain shadow or atmosphere pass, so those
// systems require no compatibility branch yet.

struct LocalClipmapPresentationConfig {
    // Geometry reaches its authored tangent presentation below the first
    // threshold and is fully spherical at and above the second threshold.
    // These values are deliberately independent from navigation controls.
    double fullyFlatAltitudeRadii{0.003};
    double fullyCurvedAltitudeRadii{0.015};
    double fullyFlatCenterFraction{0.55};
    double curvedBoundaryFraction{0.80};
};

struct LocalClipmapPresentationView {
    double altitudeMeters{};
    double planetRadiusMeters{};
    bool localNavigationRegime{};
    glm::dvec2 centerMeters{};
};

struct LocalClipmapPresentationState {
    double flattenAmount{};
    glm::dvec2 centerMeters{};
    double fullyFlatHalfExtentMeters{};
    double curvedBoundaryHalfExtentMeters{};
};

void validateLocalClipmapPresentationConfig(
    const LocalClipmapPresentationConfig& config);
void validateLocalClipmapPresentationView(
    const LocalClipmapPresentationView& view);

double localClipmapFlattenAmount(
    const LocalClipmapPresentationView& view,
    const LocalClipmapPresentationConfig& config = {});

LocalClipmapPresentationState makeLocalClipmapPresentationState(
    const LocalClipmapFootprint& footprint,
    const LocalClipmapPresentationView& view,
    std::optional<double> debugFlattenOverride = std::nullopt,
    const LocalClipmapPresentationConfig& config = {});

double localClipmapPresentationCenterMask(
    const LocalClipmapFootprint& footprint,
    const LocalClipmapPresentationState& presentation,
    const glm::dvec2& localPositionMeters);

glm::dvec3 localClipmapPresentedRelativePosition(
    const glm::dvec3& curvedRelativePosition,
    const glm::dvec3& tangentRelativePosition,
    double flattenAmount,
    double centerMask);

} // namespace wgen
