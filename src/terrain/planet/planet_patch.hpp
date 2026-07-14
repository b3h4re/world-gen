#pragma once

#include "terrain/planet/cube_sphere.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace wgen {

inline constexpr std::uint8_t MAX_PLANET_PATCH_LEVEL = 24;
static_assert(MAX_PLANET_PATCH_LEVEL < std::numeric_limits<std::uint32_t>::digits);

enum class PlanetPatchEdge : std::uint8_t {
    UMin,
    UMax,
    VMin,
    VMax,
};

struct PlanetPatchId {
    CubeSphereFace face{};
    std::uint8_t level{};
    std::uint32_t x{};
    std::uint32_t y{};

    bool operator==(const PlanetPatchId&) const = default;
};

struct PlanetPatchBounds {
    double uMin{};
    double uMax{};
    double vMin{};
    double vMax{};
};

struct PlanetPatchNeighbor {
    PlanetPatchId id{};
    PlanetPatchEdge touchingEdge{};
    bool edgeCoordinateReversed{};
};

struct PlanetFaceEdgeConnection {
    CubeSphereFace face{};
    PlanetPatchEdge edge{};
    bool coordinateReversed{};
};

struct PlanetPatchIdHash {
    std::size_t operator()(const PlanetPatchId& id) const noexcept;
};

struct PlanetPatchIdLess {
    bool operator()(const PlanetPatchId& lhs, const PlanetPatchId& rhs) const noexcept;
};

std::uint32_t patchesPerAxis(std::uint8_t level);
bool isValid(const PlanetPatchId& id);
void validate(const PlanetPatchId& id);
PlanetPatchBounds patchBounds(const PlanetPatchId& id);
std::optional<PlanetPatchId> parent(const PlanetPatchId& id);
PlanetPatchId child(const PlanetPatchId& id, std::uint8_t xBit, std::uint8_t yBit);
std::array<PlanetPatchId, 4> children(const PlanetPatchId& id);
PlanetFaceEdgeConnection faceEdgeConnection(
    CubeSphereFace face,
    PlanetPatchEdge edge);
PlanetPatchNeighbor sameLevelNeighbor(
    const PlanetPatchId& id,
    PlanetPatchEdge edge);

} // namespace wgen
