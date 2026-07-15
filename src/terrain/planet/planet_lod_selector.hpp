#pragma once

#include "terrain/planet/planet_patch.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace wgen {

inline constexpr std::uint8_t DEFAULT_PLANET_MAX_LOD = 10;
inline constexpr std::uint32_t DEFAULT_PLANET_PATCH_QUADS = 32;
inline constexpr double DEFAULT_PLANET_TARGET_ERROR_PIXELS = 2.0;
inline constexpr double DEFAULT_PLANET_LOD_MERGE_RATIO = 0.7;
inline constexpr std::size_t DEFAULT_PLANET_RESIDENT_PATCH_BUDGET = 1024;
inline constexpr std::size_t DEFAULT_PLANET_PATCH_UPLOAD_BUDGET = 8;
inline constexpr std::size_t DEFAULT_PLANET_SELECTED_LEAF_BUDGET =
    DEFAULT_PLANET_RESIDENT_PATCH_BUDGET - DEFAULT_PLANET_PATCH_UPLOAD_BUDGET;

struct PlanetLodConfig {
    std::uint8_t minimumLevel{0};
    std::uint8_t maximumLevel{DEFAULT_PLANET_MAX_LOD};
    double targetErrorPixels{DEFAULT_PLANET_TARGET_ERROR_PIXELS};
    double mergeRatio{DEFAULT_PLANET_LOD_MERGE_RATIO};
    std::uint32_t patchQuadCount{DEFAULT_PLANET_PATCH_QUADS};
    std::size_t selectedLeafBudget{DEFAULT_PLANET_SELECTED_LEAF_BUDGET};
};

struct PlanetLodView {
    glm::dvec3 position{};
    glm::dvec3 forward{0.0, 0.0, -1.0};
    glm::dvec3 right{1.0, 0.0, 0.0};
    glm::dvec3 up{0.0, 1.0, 0.0};
    double verticalFov{};
    double aspectRatio{};
    double nearPlane{};
    double farPlane{};
    std::uint32_t viewportHeight{};
};

struct PlanetLodSurface {
    double radius{1.0};
    double maximumDisplacement{};
};

struct PlanetLodSelection {
    std::vector<PlanetPatchId> leaves{};
    std::vector<PlanetPatchId> visibleLeaves{};
};

void validatePlanetLodConfig(const PlanetLodConfig& config);
void validatePlanetLodView(const PlanetLodView& view);
void validatePlanetLodSurface(const PlanetLodSurface& surface);

double planetPatchScreenErrorPixels(
    const PlanetPatchId& id,
    const PlanetLodView& view,
    const PlanetLodSurface& surface,
    std::uint32_t patchQuadCount = DEFAULT_PLANET_PATCH_QUADS);

bool isPlanetPatchVisible(
    const PlanetPatchId& id,
    const PlanetLodView& view,
    const PlanetLodSurface& surface);

class PlanetLodSelector {
public:
    PlanetLodSelection select(
        const PlanetLodView& view,
        const PlanetLodSurface& surface,
        const PlanetLodConfig& config,
        std::span<const PlanetPatchId> previousLeaves = {}) const;

    std::vector<PlanetPatchId> visibleLeaves(
        std::span<const PlanetPatchId> leaves,
        const PlanetLodView& view,
        const PlanetLodSurface& surface) const;
};

} // namespace wgen
