#include "terrain/planet/local_clipmap_presentation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wgen {
namespace {

bool isFinite(const glm::dvec2& value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool isFinite(const glm::dvec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

double smoothUnit(double value) {
    const double clamped = std::clamp(value, 0.0, 1.0);
    return clamped * clamped * (3.0 - 2.0 * clamped);
}

void validateBlend(double value, const char* message) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw std::invalid_argument{message};
    }
}

} // namespace

void validateLocalClipmapPresentationConfig(
        const LocalClipmapPresentationConfig& config) {
    if (!std::isfinite(config.fullyFlatAltitudeRadii) ||
            !std::isfinite(config.fullyCurvedAltitudeRadii) ||
            config.fullyFlatAltitudeRadii < 0.0 ||
            config.fullyCurvedAltitudeRadii <=
                config.fullyFlatAltitudeRadii ||
            !std::isfinite(config.fullyFlatCenterFraction) ||
            !std::isfinite(config.curvedBoundaryFraction) ||
            config.fullyFlatCenterFraction <= 0.0 ||
            config.curvedBoundaryFraction <=
                config.fullyFlatCenterFraction ||
            config.curvedBoundaryFraction >= 1.0) {
        throw std::invalid_argument{
            "local clipmap presentation config is invalid"};
    }
}

void validateLocalClipmapPresentationView(
        const LocalClipmapPresentationView& view) {
    if (!std::isfinite(view.altitudeMeters) || view.altitudeMeters < 0.0 ||
            !std::isfinite(view.planetRadiusMeters) ||
            view.planetRadiusMeters <= 0.0 ||
            !isFinite(view.centerMeters)) {
        throw std::invalid_argument{
            "local clipmap presentation view is invalid"};
    }
}

double localClipmapFlattenAmount(
        const LocalClipmapPresentationView& view,
        const LocalClipmapPresentationConfig& config) {
    validateLocalClipmapPresentationView(view);
    validateLocalClipmapPresentationConfig(config);
    if (!view.localNavigationRegime) {
        return 0.0;
    }
    const double altitudeRadii =
        view.altitudeMeters / view.planetRadiusMeters;
    const double linear =
        (config.fullyCurvedAltitudeRadii - altitudeRadii) /
        (config.fullyCurvedAltitudeRadii -
            config.fullyFlatAltitudeRadii);
    return smoothUnit(linear);
}

LocalClipmapPresentationState makeLocalClipmapPresentationState(
        const LocalClipmapFootprint& footprint,
        const LocalClipmapPresentationView& view,
        std::optional<double> debugFlattenOverride,
        const LocalClipmapPresentationConfig& config) {
    validateLocalClipmapFootprint(footprint);
    validateLocalClipmapPresentationView(view);
    validateLocalClipmapPresentationConfig(config);
    if (debugFlattenOverride) {
        validateBlend(
            *debugFlattenOverride,
            "local clipmap debug flatten override is invalid");
    }
    const glm::dvec2 centerOffset =
        glm::abs(view.centerMeters - footprint.centerMeters);
    const double availableHalfExtent = footprint.innerHalfExtentMeters -
        std::max(centerOffset.x, centerOffset.y);
    if (availableHalfExtent <= 0.0) {
        throw std::invalid_argument{
            "local clipmap presentation center lies outside its footprint"};
    }
    return {
        .flattenAmount = debugFlattenOverride.value_or(
            localClipmapFlattenAmount(view, config)),
        .centerMeters = view.centerMeters,
        .fullyFlatHalfExtentMeters = availableHalfExtent *
            config.fullyFlatCenterFraction,
        .curvedBoundaryHalfExtentMeters = availableHalfExtent *
            config.curvedBoundaryFraction,
    };
}

double localClipmapPresentationCenterMask(
        const LocalClipmapFootprint& footprint,
        const LocalClipmapPresentationState& presentation,
        const glm::dvec2& localPositionMeters) {
    validateLocalClipmapFootprint(footprint);
    const glm::dvec2 presentationCenterOffset =
        glm::abs(presentation.centerMeters - footprint.centerMeters);
    const double maximumCenterOffset = std::max(
        presentationCenterOffset.x,
        presentationCenterOffset.y);
    if (!isFinite(localPositionMeters) ||
            !isFinite(presentation.centerMeters) ||
            !std::isfinite(presentation.flattenAmount) ||
            !std::isfinite(presentation.fullyFlatHalfExtentMeters) ||
            !std::isfinite(
                presentation.curvedBoundaryHalfExtentMeters) ||
            presentation.flattenAmount < 0.0 ||
            presentation.flattenAmount > 1.0 ||
            presentation.fullyFlatHalfExtentMeters <= 0.0 ||
            presentation.curvedBoundaryHalfExtentMeters <=
                presentation.fullyFlatHalfExtentMeters ||
            maximumCenterOffset +
                presentation.curvedBoundaryHalfExtentMeters >=
                footprint.innerHalfExtentMeters) {
        throw std::invalid_argument{
            "local clipmap presentation state is invalid"};
    }
    const glm::dvec2 relative =
        glm::abs(localPositionMeters - presentation.centerMeters);
    const double distance = std::max(relative.x, relative.y);
    const double linear =
        (presentation.curvedBoundaryHalfExtentMeters - distance) /
        (presentation.curvedBoundaryHalfExtentMeters -
            presentation.fullyFlatHalfExtentMeters);
    return smoothUnit(linear);
}

glm::dvec3 localClipmapPresentedRelativePosition(
        const glm::dvec3& curvedRelativePosition,
        const glm::dvec3& tangentRelativePosition,
        double flattenAmount,
        double centerMask) {
    if (!isFinite(curvedRelativePosition) ||
            !isFinite(tangentRelativePosition)) {
        throw std::invalid_argument{
            "local clipmap presentation positions must be finite"};
    }
    validateBlend(
        flattenAmount,
        "local clipmap flatten amount is invalid");
    validateBlend(
        centerMask,
        "local clipmap presentation center mask is invalid");
    return glm::mix(
        curvedRelativePosition,
        tangentRelativePosition,
        flattenAmount * centerMask);
}

} // namespace wgen
