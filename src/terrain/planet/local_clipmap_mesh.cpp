#include "terrain/planet/local_clipmap_mesh.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace lve {
namespace {

bool isFinite(const glm::dvec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool fitsFloat(const glm::dvec3& value) {
    const double maximum = std::numeric_limits<float>::max();
    return isFinite(value) && std::abs(value.x) <= maximum &&
        std::abs(value.y) <= maximum && std::abs(value.z) <= maximum;
}

} // namespace

LocalClipmapLevelMeshData buildLocalClipmapLevelMesh(
        const wgen::LocalPlanetFrame& frame,
        const wgen::LocalClipmapTopology& topology,
        const wgen::LocalClipmapCacheOrigin& origin,
        const wgen::LocalClipmapUpdateIdentity& identity,
        std::span<const float> heightsMeters) {
    wgen::validateLocalPlanetFrame(frame);
    wgen::validateLocalClipmapConfig(topology.config);
    if (identity.terrainEpoch == 0 || identity.requestRevision == 0 ||
            identity.level >= topology.config.levelCount) {
        throw std::invalid_argument{
            "local clipmap mesh identity is invalid"};
    }
    if (origin.level != identity.level ||
            origin.frameRevision != identity.localFrameRevision ||
            origin.centerSample != identity.snappedGridOrigin) {
        throw std::invalid_argument{
            "local clipmap mesh origin and identity disagree"};
    }
    if (origin.frameRevision != frame.revision) {
        throw std::invalid_argument{
            "local clipmap mesh origin belongs to another local frame"};
    }
    if (heightsMeters.size() != topology.vertices.size()) {
        throw std::invalid_argument{
            "local clipmap mesh requires one height per grid vertex"};
    }

    LocalClipmapLevelMeshData result{
        .level = origin.level,
        .identity = identity,
        .globalOrigin = frame.anchorDirection,
    };
    result.vertices.reserve(topology.vertices.size());
    const double spacing = wgen::localClipmapCellSpacing(
        topology.config,
        origin.level);
    for (std::size_t index = 0; index < topology.vertices.size(); ++index) {
        const float heightMeters = heightsMeters[index];
        if (!std::isfinite(heightMeters)) {
            throw std::invalid_argument{
                "local clipmap mesh height must be finite"};
        }
        const glm::dvec2 localPositionMeters =
            wgen::localClipmapVertexPosition(
                origin,
                topology.vertices[index],
                spacing);
        const glm::dvec3 direction = wgen::localPlanetSurfaceDirection(
            frame,
            localPositionMeters);
        const glm::dvec3 globalPositionInRadii =
            wgen::localPlanetCurvedPosition(
                frame,
                direction,
                heightMeters) / frame.planetRadiusMeters;
        const glm::dvec3 localPositionInRadii =
            globalPositionInRadii - result.globalOrigin;
        if (!fitsFloat(localPositionInRadii)) {
            throw std::overflow_error{
                "local clipmap mesh position exceeds float render space"};
        }
        const glm::vec3 position{localPositionInRadii};
        result.vertices.push_back({
            .position = position,
            .height = heightMeters,
            // The local pipeline does not patch-morph. Reuse this otherwise
            // dormant attribute for the shared tangent-space coverage map.
            .parentPosition = {
                static_cast<float>(localPositionMeters.x),
                static_cast<float>(localPositionMeters.y),
                0.0F,
            },
            .parentHeight = heightMeters,
        });
    }
    return result;
}

} // namespace lve
