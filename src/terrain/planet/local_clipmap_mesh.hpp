#pragma once

#include "renderer/objects/mesh_3d.hpp"
#include "terrain/planet/local_clipmap_residency.hpp"
#include "terrain/planet/local_clipmap_topology.hpp"
#include "terrain/planet/local_planet_frame.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace lve {

// CPU-position fallback for the first renderer path. Positions are compact
// offsets in planet radii from globalOrigin; the renderer still owns the
// immutable topology and dynamic GPU height buffers so a later vertex-shader
// displacement path can preserve the same update contract.
struct LocalClipmapLevelMeshData {
    std::uint32_t level{};
    wgen::LocalClipmapUpdateIdentity identity{};
    glm::dvec3 globalOrigin{};
    std::vector<Vertex3d> vertices{};
};

struct LocalClipmapLevelCacheState {
    std::uint32_t level{};
    bool hasActiveCache{};
    bool activeCacheIsCurrent{};
};

struct LocalClipmapMeshUpdate {
    std::vector<LocalClipmapLevelMeshData> upserts{};
    std::vector<LocalClipmapLevelCacheState> cacheStates{};
};

LocalClipmapLevelMeshData buildLocalClipmapLevelMesh(
    const wgen::LocalPlanetFrame& frame,
    const wgen::LocalClipmapTopology& topology,
    const wgen::LocalClipmapCacheOrigin& origin,
    const wgen::LocalClipmapUpdateIdentity& identity,
    std::span<const float> heightsMeters);

} // namespace lve
