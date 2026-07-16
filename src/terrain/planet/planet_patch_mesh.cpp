#include "planet_patch_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace lve {

namespace {

constexpr std::array PLANET_PATCH_EDGES{
    wgen::PlanetPatchEdge::UMin,
    wgen::PlanetPatchEdge::UMax,
    wgen::PlanetPatchEdge::VMin,
    wgen::PlanetPatchEdge::VMax,
};

struct PlanetPatchTopologyCounts {
    std::uint64_t surfaceVertices{};
    std::uint64_t vertices{};
    std::uint64_t surfaceIndices{};
    std::uint64_t indices{};
};

PlanetPatchTopologyCounts planetPatchTopologyCounts(std::uint32_t quadCount) {
    if (quadCount == 0) {
        throw std::invalid_argument{"planet patch topology requires positive quads"};
    }
    constexpr std::uint64_t MAX_BUFFER_COUNT =
        std::numeric_limits<std::uint32_t>::max();
    const std::uint64_t quads = quadCount;
    const std::uint64_t samplesPerAxis = quads + 1;
    if (samplesPerAxis > MAX_BUFFER_COUNT / samplesPerAxis ||
            quads > MAX_BUFFER_COUNT / quads) {
        throw std::overflow_error{"planet patch topology exceeds uint32 indexing"};
    }
    const std::uint64_t surfaceVertices = samplesPerAxis * samplesPerAxis;
    const std::uint64_t skirtVertices = 4 * samplesPerAxis;
    const std::uint64_t quadArea = quads * quads;
    if (skirtVertices > MAX_BUFFER_COUNT - surfaceVertices ||
            quadArea > MAX_BUFFER_COUNT / 6) {
        throw std::overflow_error{"planet patch topology exceeds uint32 indexing"};
    }
    const std::uint64_t surfaceIndices = quadArea * 6;
    const std::uint64_t skirtIndices = 24 * quads;
    if (skirtIndices > MAX_BUFFER_COUNT - surfaceIndices) {
        throw std::overflow_error{"planet patch topology exceeds uint32 indexing"};
    }
    return {
        .surfaceVertices = surfaceVertices,
        .vertices = surfaceVertices + skirtVertices,
        .surfaceIndices = surfaceIndices,
        .indices = surfaceIndices + skirtIndices,
    };
}

std::size_t edgeIndex(wgen::PlanetPatchEdge edge) {
    switch (edge) {
        case wgen::PlanetPatchEdge::UMin: return 0;
        case wgen::PlanetPatchEdge::UMax: return 1;
        case wgen::PlanetPatchEdge::VMin: return 2;
        case wgen::PlanetPatchEdge::VMax: return 3;
    }
    throw std::invalid_argument{"planet patch mesh edge is invalid"};
}

std::pair<std::uint32_t, std::uint32_t> edgeCoordinates(
        wgen::PlanetPatchEdge edge,
        std::uint32_t quadCount,
        std::uint32_t sample) {
    switch (edge) {
        case wgen::PlanetPatchEdge::UMin: return {0, sample};
        case wgen::PlanetPatchEdge::UMax: return {quadCount, sample};
        case wgen::PlanetPatchEdge::VMin: return {sample, 0};
        case wgen::PlanetPatchEdge::VMax: return {sample, quadCount};
    }
    throw std::invalid_argument{"planet patch mesh edge is invalid"};
}

std::uint32_t remappedSurfaceVertexIndex(
        std::uint32_t quadCount,
        std::uint32_t localX,
        std::uint32_t localY,
        wgen::PlanetPatchEdgeMask stitchMask) {
    if ((stitchMask & wgen::planetPatchEdgeBit(wgen::PlanetPatchEdge::UMin)) != 0 &&
            localX == 0 && localY % 2 == 1) {
        --localY;
    }
    if ((stitchMask & wgen::planetPatchEdgeBit(wgen::PlanetPatchEdge::UMax)) != 0 &&
            localX == quadCount && localY % 2 == 1) {
        --localY;
    }
    if ((stitchMask & wgen::planetPatchEdgeBit(wgen::PlanetPatchEdge::VMin)) != 0 &&
            localY == 0 && localX % 2 == 1) {
        --localX;
    }
    if ((stitchMask & wgen::planetPatchEdgeBit(wgen::PlanetPatchEdge::VMax)) != 0 &&
            localY == quadCount && localX % 2 == 1) {
        --localX;
    }
    return planetPatchSurfaceVertexIndex(quadCount, localX, localY);
}

struct ParentGridVertex {
    glm::vec3 position{};
    float height{};
};

ParentGridVertex interpolateParentGrid(
        const std::vector<ParentGridVertex>& grid,
        std::uint32_t quadCount,
        double gridX,
        double gridY) {
    const std::uint32_t x0 = std::min(
        static_cast<std::uint32_t>(std::floor(gridX)),
        quadCount - 1);
    const std::uint32_t y0 = std::min(
        static_cast<std::uint32_t>(std::floor(gridY)),
        quadCount - 1);
    const double xWeight = gridX - static_cast<double>(x0);
    const double yWeight = gridY - static_cast<double>(y0);
    const auto& topLeft = grid[planetPatchSurfaceVertexIndex(quadCount, x0, y0)];
    const auto& topRight = grid[planetPatchSurfaceVertexIndex(quadCount, x0 + 1, y0)];
    const auto& bottomLeft = grid[planetPatchSurfaceVertexIndex(quadCount, x0, y0 + 1)];
    const auto& bottomRight = grid[planetPatchSurfaceVertexIndex(quadCount, x0 + 1, y0 + 1)];

    ParentGridVertex result;
    if (xWeight >= yWeight) {
        const float topLeftWeight = static_cast<float>(1.0 - xWeight);
        const float topRightWeight = static_cast<float>(xWeight - yWeight);
        const float bottomRightWeight = static_cast<float>(yWeight);
        result.position = topLeft.position * topLeftWeight +
            topRight.position * topRightWeight +
            bottomRight.position * bottomRightWeight;
        result.height = topLeft.height * topLeftWeight +
            topRight.height * topRightWeight +
            bottomRight.height * bottomRightWeight;
    } else {
        const float topLeftWeight = static_cast<float>(1.0 - yWeight);
        const float bottomRightWeight = static_cast<float>(xWeight);
        const float bottomLeftWeight = static_cast<float>(yWeight - xWeight);
        result.position = topLeft.position * topLeftWeight +
            bottomRight.position * bottomRightWeight +
            bottomLeft.position * bottomLeftWeight;
        result.height = topLeft.height * topLeftWeight +
            bottomRight.height * bottomRightWeight +
            bottomLeft.height * bottomLeftWeight;
    }
    return result;
}

} // namespace

std::uint32_t planetPatchSurfaceVertexIndex(
        std::uint32_t quadCount,
        std::uint32_t localX,
        std::uint32_t localY) {
    if (localX > quadCount || localY > quadCount) {
        throw std::out_of_range{"planet patch surface vertex coordinate is out of range"};
    }
    const std::uint64_t result =
        std::uint64_t{localY} * (std::uint64_t{quadCount} + 1) + localX;
    if (result > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error{"planet patch surface vertex index overflows uint32"};
    }
    return static_cast<std::uint32_t>(result);
}

std::uint32_t planetPatchSkirtVertexIndex(
        std::uint32_t quadCount,
        wgen::PlanetPatchEdge edge,
        std::uint32_t edgeSample) {
    if (edgeSample > quadCount) {
        throw std::out_of_range{"planet patch skirt sample is out of range"};
    }
    const std::uint64_t samplesPerAxis = std::uint64_t{quadCount} + 1;
    if (samplesPerAxis > std::numeric_limits<std::uint32_t>::max() /
            samplesPerAxis) {
        throw std::overflow_error{"planet patch skirt vertex index overflows uint32"};
    }
    const std::uint64_t surfaceCount = samplesPerAxis * samplesPerAxis;
    const std::uint64_t result = surfaceCount +
        edgeIndex(edge) * samplesPerAxis + edgeSample;
    if (result > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error{"planet patch skirt vertex index overflows uint32"};
    }
    return static_cast<std::uint32_t>(result);
}

namespace {

std::vector<std::uint32_t> buildPlanetPatchIndices(
        std::uint32_t quadCount,
        wgen::PlanetPatchEdgeMask mask) {
    if (quadCount == 0) {
        throw std::invalid_argument{"planet patch indices require positive quads"};
    }
    const PlanetPatchTopologyCounts counts =
        planetPatchTopologyCounts(quadCount);

    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(counts.indices));
    for (std::uint32_t y = 0; y < quadCount; ++y) {
        for (std::uint32_t x = 0; x < quadCount; ++x) {
            const std::uint32_t topLeft =
                remappedSurfaceVertexIndex(quadCount, x, y, mask);
            const std::uint32_t topRight =
                remappedSurfaceVertexIndex(quadCount, x + 1, y, mask);
            const std::uint32_t bottomLeft =
                remappedSurfaceVertexIndex(quadCount, x, y + 1, mask);
            const std::uint32_t bottomRight =
                remappedSurfaceVertexIndex(quadCount, x + 1, y + 1, mask);
            indices.insert(
                indices.end(),
                {topLeft, topRight, bottomRight, topLeft, bottomRight, bottomLeft});
        }
    }

    for (const wgen::PlanetPatchEdge edge : PLANET_PATCH_EDGES) {
        const bool stitched = (mask & wgen::planetPatchEdgeBit(edge)) != 0;
        for (std::uint32_t sample = 0; sample < quadCount; ++sample) {
            const std::uint32_t sourceSample = stitched && sample % 2 == 1
                ? sample - 1
                : sample;
            const std::uint32_t targetSample = stitched && (sample + 1) % 2 == 1
                ? sample
                : sample + 1;
            const auto [sourceX, sourceY] = edgeCoordinates(edge, quadCount, sourceSample);
            const auto [targetX, targetY] = edgeCoordinates(edge, quadCount, targetSample);
            const std::uint32_t top0 = planetPatchSurfaceVertexIndex(
                quadCount, sourceX, sourceY);
            const std::uint32_t top1 = planetPatchSurfaceVertexIndex(
                quadCount, targetX, targetY);
            const std::uint32_t skirt0 = planetPatchSkirtVertexIndex(
                quadCount, edge, sourceSample);
            const std::uint32_t skirt1 = planetPatchSkirtVertexIndex(
                quadCount, edge, targetSample);
            indices.insert(
                indices.end(),
                {top0, top1, skirt1, top0, skirt1, skirt0});
        }
    }
    return indices;
}

} // namespace

PlanetPatchIndexVariants buildPlanetPatchIndexVariants(std::uint32_t quadCount) {
    PlanetPatchIndexVariants variants;
    for (std::size_t maskIndex = 0; maskIndex < variants.size(); ++maskIndex) {
        variants[maskIndex] = buildPlanetPatchIndices(
            quadCount,
            static_cast<wgen::PlanetPatchEdgeMask>(maskIndex));
    }
    return variants;
}

float sampleCubeSphereBilinear(
        const wgen::CubeSphere<float>& source,
        const wgen::PlanetSurfaceSample& surface) {
    static_cast<void>(wgen::faceID(surface.face));
    if (source.resolution() < 2) {
        throw std::invalid_argument{"cube sphere sampling requires a resolution of at least two"};
    }
    if (!std::isfinite(surface.u) || !std::isfinite(surface.v) ||
            surface.u < -1.0 || surface.u > 1.0 || surface.v < -1.0 || surface.v > 1.0) {
        throw std::invalid_argument{"cube sphere sample UV must be finite and inside the face"};
    }

    const double last = static_cast<double>(source.resolution() - 1);
    const double gridX = (surface.u + 1.0) * 0.5 * last;
    const double gridY = (surface.v + 1.0) * 0.5 * last;
    const std::size_t x0 = static_cast<std::size_t>(std::floor(gridX));
    const std::size_t y0 = static_cast<std::size_t>(std::floor(gridY));
    const std::size_t x1 = std::min(x0 + 1, source.resolution() - 1);
    const std::size_t y1 = std::min(y0 + 1, source.resolution() - 1);
    const double xWeight = gridX - static_cast<double>(x0);
    const double yWeight = gridY - static_cast<double>(y0);

    const double lower = std::lerp(
        static_cast<double>(source.at(surface.face, x0, y0)),
        static_cast<double>(source.at(surface.face, x1, y0)),
        xWeight);
    const double upper = std::lerp(
        static_cast<double>(source.at(surface.face, x0, y1)),
        static_cast<double>(source.at(surface.face, x1, y1)),
        xWeight);
    return static_cast<float>(std::lerp(lower, upper, yWeight));
}

PlanetPatchMeshData buildPlanetPatchMesh(
        const wgen::PlanetPatchId& id,
        std::uint32_t quadCount,
        float planetRadius,
        const PlanetHeightSampler& heightSampler,
        PlanetPatchMeshBuildConfig config) {
    wgen::validate(id);
    if (quadCount == 0) {
        throw std::invalid_argument{"planet patch mesh quad count must be positive"};
    }
    if (!std::isfinite(planetRadius) || planetRadius <= 0.0F) {
        throw std::invalid_argument{"planet patch mesh radius must be finite and positive"};
    }
    if (!heightSampler) {
        throw std::invalid_argument{"planet patch mesh height sampler must be callable"};
    }
    if (!std::isfinite(config.skirtDepthMeters) || config.skirtDepthMeters < 0.0F) {
        throw std::invalid_argument{"planet patch skirt depth must be finite and non-negative"};
    }
    if (id.level > 0 && quadCount % 2 != 0) {
        throw std::invalid_argument{"morphable child patches require an even quad count"};
    }

    const PlanetPatchTopologyCounts counts =
        planetPatchTopologyCounts(quadCount);

    PlanetPatchMeshData result;
    result.id = id;
    result.quadCount = quadCount;
    result.surfaceVertexCount = static_cast<std::size_t>(counts.surfaceVertices);
    result.surfaceIndexCount = static_cast<std::size_t>(counts.surfaceIndices);
    result.skirtDepthMeters = config.skirtDepthMeters;
    result.vertices.reserve(static_cast<std::size_t>(counts.vertices));
    result.indices.reserve(static_cast<std::size_t>(counts.indices));

    const PlanetHeightSampler& parentHeightSampler = config.parentHeightSampler
        ? config.parentHeightSampler
        : heightSampler;
    std::vector<ParentGridVertex> parentGrid;
    std::optional<wgen::PlanetPatchId> parentId;
    if (id.level > 0) {
        parentId = wgen::parent(id);
        parentGrid.reserve(static_cast<std::size_t>(counts.surfaceVertices));
        for (std::uint32_t parentY = 0; parentY <= quadCount; ++parentY) {
            for (std::uint32_t parentX = 0; parentX <= quadCount; ++parentX) {
                const wgen::PlanetSurfaceSample parentSurface = wgen::patchSurfaceSample(
                    *parentId,
                    quadCount,
                    parentX,
                    parentY);
                const float parentHeight = parentHeightSampler(parentSurface);
                if (!std::isfinite(parentHeight)) {
                    throw std::invalid_argument{"planet patch parent height must be finite"};
                }
                parentGrid.push_back({
                    glm::vec3{parentSurface.direction} *
                        (parentHeight + planetRadius) / planetRadius,
                    parentHeight,
                });
            }
        }
    }

    for (std::uint32_t localY = 0; localY <= quadCount; ++localY) {
        for (std::uint32_t localX = 0; localX <= quadCount; ++localX) {
            const wgen::PlanetSurfaceSample surface =
                wgen::patchSurfaceSample(id, quadCount, localX, localY);
            const float height = heightSampler(surface);
            if (!std::isfinite(height)) {
                throw std::invalid_argument{"planet patch mesh height must be finite"};
            }
            const glm::vec3 position = glm::vec3{surface.direction} *
                (height + planetRadius) / planetRadius;
            ParentGridVertex parentRepresentation{position, height};
            if (parentId) {
                const double parentGridX =
                    static_cast<double>(id.x & 1U) * quadCount / 2.0 +
                    static_cast<double>(localX) / 2.0;
                const double parentGridY =
                    static_cast<double>(id.y & 1U) * quadCount / 2.0 +
                    static_cast<double>(localY) / 2.0;
                parentRepresentation = interpolateParentGrid(
                    parentGrid,
                    quadCount,
                    parentGridX,
                    parentGridY);
            }
            result.vertices.push_back({
                position,
                height,
                parentRepresentation.position,
                parentRepresentation.height,
            });
        }
    }

    const float normalizedSkirtDepth = config.skirtDepthMeters / planetRadius;
    for (const wgen::PlanetPatchEdge edge : PLANET_PATCH_EDGES) {
        for (std::uint32_t sample = 0; sample <= quadCount; ++sample) {
            const auto [localX, localY] = edgeCoordinates(edge, quadCount, sample);
            const wgen::PlanetSurfaceSample surface = wgen::patchSurfaceSample(
                id,
                quadCount,
                localX,
                localY);
            const Vertex3d& top = result.vertices[
                planetPatchSurfaceVertexIndex(quadCount, localX, localY)];
            glm::vec3 parentDirection = glm::vec3{surface.direction};
            if (glm::length(top.parentPosition) > 0.0F) {
                parentDirection = glm::normalize(top.parentPosition);
            }
            result.vertices.push_back({
                top.position - glm::vec3{surface.direction} * normalizedSkirtDepth,
                top.height - config.skirtDepthMeters,
                top.parentPosition - parentDirection * normalizedSkirtDepth,
                top.parentHeight - config.skirtDepthMeters,
            });
        }
    }

    result.indices = buildPlanetPatchIndices(quadCount, 0);

    return result;
}

PlanetPatchMeshData buildRequestedPlanetPatchMesh(
        const PlanetPatchMeshRequest& request,
        std::uint32_t quadCount,
        float planetRadius,
        const PlanetHeightSampler& heightSampler,
        PlanetPatchMeshBuildConfig config) {
    PlanetPatchMeshData result = buildPlanetPatchMesh(
        request.id,
        quadCount,
        planetRadius,
        heightSampler,
        std::move(config));
    result.version = request.version;
    return result;
}

std::vector<PlanetPatchMeshData> buildFixedLevelPlanetPatchMeshes(
        const wgen::CubeSphere<float>& source,
        std::uint8_t level) {
    if (source.resolution() < 2) {
        throw std::invalid_argument{"fixed planet patches require a populated cube sphere"};
    }

    return buildFixedLevelPlanetPatchMeshes(
        source.radius(),
        level,
        [&source](const wgen::PlanetSurfaceSample& surface) {
            return sampleCubeSphereBilinear(source, surface);
        });
}

std::vector<PlanetPatchMeshData> buildFixedLevelPlanetPatchMeshes(
        float planetRadius,
        std::uint8_t level,
        const PlanetHeightSampler& heightSampler) {
    return buildFixedLevelPlanetPatchMeshes(
        planetRadius,
        level,
        PlanetPatchVersion{},
        heightSampler);
}

std::vector<PlanetPatchMeshData> buildFixedLevelPlanetPatchMeshes(
        float planetRadius,
        std::uint8_t level,
        const PlanetPatchVersion& version,
        const PlanetHeightSampler& heightSampler) {
    if (level > MAX_FIXED_PLANET_PATCH_LEVEL) {
        throw std::invalid_argument{"fixed planet patch level exceeds the debug maximum"};
    }
    if (!std::isfinite(planetRadius) || planetRadius <= 0.0F) {
        throw std::invalid_argument{"fixed planet patches require a finite positive radius"};
    }
    if (!heightSampler) {
        throw std::invalid_argument{"fixed planet patches require a height sampler"};
    }

    const std::vector<wgen::PlanetPatchId> ids = fixedLevelPlanetPatchIds(level);
    std::vector<PlanetPatchMeshData> result;
    result.reserve(ids.size());
    for (const wgen::PlanetPatchId& id : ids) {
        result.push_back(buildRequestedPlanetPatchMesh(
            PlanetPatchMeshRequest{.id = id, .version = version},
            PLANET_PATCH_QUADS,
            planetRadius,
            heightSampler));
    }
    return result;
}

std::vector<wgen::PlanetPatchId> fixedLevelPlanetPatchIds(std::uint8_t level) {
    if (level > MAX_FIXED_PLANET_PATCH_LEVEL) {
        throw std::invalid_argument{"fixed planet patch level exceeds the debug maximum"};
    }

    const std::uint32_t count = wgen::patchesPerAxis(level);
    std::vector<wgen::PlanetPatchId> result;
    result.reserve(wgen::FACES.size() * static_cast<std::size_t>(count) * count);
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::uint32_t y = 0; y < count; ++y) {
            for (std::uint32_t x = 0; x < count; ++x) {
                result.push_back({face, level, x, y});
            }
        }
    }
    return result;
}

std::vector<wgen::PlanetPatchId> planetPatchIdDifference(
        std::span<const wgen::PlanetPatchId> previousIds,
        std::span<const wgen::PlanetPatchId> desiredIds) {
    std::unordered_set<wgen::PlanetPatchId, wgen::PlanetPatchIdHash> desired;
    desired.reserve(desiredIds.size());
    for (const wgen::PlanetPatchId& id : desiredIds) {
        wgen::validate(id);
        desired.insert(id);
    }

    std::vector<wgen::PlanetPatchId> result;
    result.reserve(previousIds.size());
    for (const wgen::PlanetPatchId& id : previousIds) {
        wgen::validate(id);
        if (!desired.contains(id)) {
            result.push_back(id);
        }
    }
    std::sort(result.begin(), result.end(), wgen::PlanetPatchIdLess{});
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

PlanetPatchMeshBatch makePlanetPatchMeshBatch(
        std::vector<PlanetPatchMeshData> upserts,
        std::span<const wgen::PlanetPatchId> previousIds,
        std::uint64_t terrainEpoch,
        std::uint64_t requestRevision) {
    std::vector<wgen::PlanetPatchId> desiredIds;
    desiredIds.reserve(upserts.size());
    for (const PlanetPatchMeshData& upsert : upserts) {
        desiredIds.push_back(upsert.id);
    }

    PlanetPatchMeshBatch result{
        .terrainEpoch = terrainEpoch,
        .requestRevision = requestRevision,
        .upserts = std::move(upserts),
    };
    for (const wgen::PlanetPatchId& id : planetPatchIdDifference(previousIds, desiredIds)) {
        result.removals.push_back({
            .id = id,
            .terrainEpoch = terrainEpoch,
            .requestRevision = requestRevision,
        });
    }
    validatePlanetPatchMeshBatch(result);
    return result;
}

void validatePlanetPatchMeshBatch(const PlanetPatchMeshBatch& batch) {
    if (batch.terrainEpoch == 0 || batch.requestRevision == 0) {
        throw std::invalid_argument{"planet patch batch versions must be initialized"};
    }

    std::unordered_set<wgen::PlanetPatchId, wgen::PlanetPatchIdHash> upsertIds;
    upsertIds.reserve(batch.upserts.size());
    for (const PlanetPatchMeshData& upsert : batch.upserts) {
        wgen::validate(upsert.id);
        if (upsert.vertices.empty() || upsert.indices.empty()) {
            throw std::invalid_argument{"planet patch upsert mesh must not be empty"};
        }
        if ((upsert.id.level > 0 && upsert.quadCount % 2 != 0) ||
                !std::isfinite(upsert.skirtDepthMeters) ||
                upsert.skirtDepthMeters < 0.0F) {
            throw std::invalid_argument{"planet patch upsert mesh metadata is invalid"};
        }
        const PlanetPatchTopologyCounts counts =
            planetPatchTopologyCounts(upsert.quadCount);
        if (upsert.surfaceVertexCount != counts.surfaceVertices ||
                upsert.vertices.size() != counts.vertices ||
                upsert.surfaceIndexCount != counts.surfaceIndices ||
                upsert.indices.size() != counts.indices ||
                std::any_of(
                    upsert.indices.begin(),
                    upsert.indices.end(),
                    [&upsert](std::uint32_t index) {
                        return index >= upsert.vertices.size();
                    })) {
            throw std::invalid_argument{"planet patch upsert mesh topology is invalid"};
        }
        if (upsert.version.terrainEpoch != batch.terrainEpoch ||
                upsert.version.requestRevision != batch.requestRevision) {
            throw std::invalid_argument{"planet patch upsert version disagrees with its batch"};
        }
        if (!upsertIds.insert(upsert.id).second) {
            throw std::invalid_argument{"planet patch batch contains duplicate upserts"};
        }
    }

    std::unordered_set<wgen::PlanetPatchId, wgen::PlanetPatchIdHash> removalIds;
    removalIds.reserve(batch.removals.size());
    for (const PlanetPatchRemoval& removal : batch.removals) {
        wgen::validate(removal.id);
        if (removal.terrainEpoch != batch.terrainEpoch ||
                removal.requestRevision != batch.requestRevision) {
            throw std::invalid_argument{"planet patch removal version disagrees with its batch"};
        }
        if (upsertIds.contains(removal.id)) {
            throw std::invalid_argument{"planet patch batch contains overlapping operations"};
        }
        if (!removalIds.insert(removal.id).second) {
            throw std::invalid_argument{"planet patch batch contains duplicate removals"};
        }
    }
}

bool isCurrentPlanetPatchMeshBatch(
        const PlanetPatchMeshBatch& batch,
        std::uint64_t desiredRequestRevision) noexcept {
    return batch.requestRevision == desiredRequestRevision;
}

bool discardStalePlanetPatchMeshBatch(
        std::optional<PlanetPatchMeshBatch>& batch,
        std::uint64_t desiredRequestRevision) noexcept {
    if (batch && !isCurrentPlanetPatchMeshBatch(*batch, desiredRequestRevision)) {
        batch.reset();
        return true;
    }
    return false;
}

PlanetPatchBatchEpochAction planetPatchBatchEpochAction(
        std::uint64_t batchTerrainEpoch,
        std::uint64_t activeTerrainEpoch) noexcept {
    if (batchTerrainEpoch < activeTerrainEpoch) {
        return PlanetPatchBatchEpochAction::Discard;
    }
    if (batchTerrainEpoch > activeTerrainEpoch) {
        return PlanetPatchBatchEpochAction::Replace;
    }
    return PlanetPatchBatchEpochAction::Apply;
}

bool shouldApplyPlanetPatchUpsert(
        const PlanetPatchVersion& incoming,
        const PlanetPatchVersion& resident) noexcept {
    if (incoming.terrainEpoch != resident.terrainEpoch ||
            incoming.requestRevision < resident.requestRevision ||
            incoming.dependencyRevision < resident.dependencyRevision) {
        return false;
    }
    return incoming.requestRevision > resident.requestRevision ||
        incoming.dependencyRevision > resident.dependencyRevision;
}

bool shouldApplyPlanetPatchRemoval(
        const PlanetPatchRemoval& removal,
        const PlanetPatchVersion& resident) noexcept {
    return removal.terrainEpoch == resident.terrainEpoch &&
        removal.requestRevision >= resident.requestRevision;
}

void appendCubeSphereMesh(
        const wgen::CubeSphere<float>& cubeSphere,
        std::vector<Vertex3d>& vertices,
        std::vector<std::uint32_t>& indices) {
    const std::size_t resolution = cubeSphere.resolution();
    if (resolution < 2) {
        throw std::invalid_argument{"cube sphere mesh requires a face resolution of at least two"};
    }
    if (resolution > std::numeric_limits<std::uint32_t>::max() / resolution ||
            resolution * resolution > std::numeric_limits<std::uint32_t>::max() / wgen::FACES.size()) {
        throw std::overflow_error{"cube sphere mesh vertex count overflows uint32_t indices"};
    }

    vertices.reserve(vertices.size() + wgen::FACES.size() * resolution * resolution);
    indices.reserve(indices.size() + wgen::FACES.size() * (resolution - 1) * (resolution - 1) * 6);

    const float radius = cubeSphere.radius();
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < resolution; ++y) {
            for (std::size_t x = 0; x < resolution; ++x) {
                const float height = cubeSphere.at(face, x, y);
                const glm::vec3 position = cubeSphere.pointUnitDir(face, x, y) *
                    (height + radius) / radius;
                vertices.push_back({
                    position,
                    height,
                    position,
                    height,
                });
            }
        }
    }

    for (const wgen::CubeSphereFace face : wgen::FACES) {
        const std::size_t faceOffset = wgen::faceID(face) * resolution * resolution;
        for (std::size_t y = 0; y + 1 < resolution; ++y) {
            for (std::size_t x = 0; x + 1 < resolution; ++x) {
                const auto topLeft = static_cast<std::uint32_t>(faceOffset + y * resolution + x);
                const auto topRight = static_cast<std::uint32_t>(faceOffset + y * resolution + x + 1);
                const auto bottomLeft = static_cast<std::uint32_t>(faceOffset + (y + 1) * resolution + x);
                const auto bottomRight = static_cast<std::uint32_t>(faceOffset + (y + 1) * resolution + x + 1);
                indices.insert(
                    indices.end(),
                    {topLeft, topRight, bottomRight, topLeft, bottomRight, bottomLeft});
            }
        }
    }
}

} // namespace lve
