#include "planet_patch_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lve {

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
        const PlanetHeightSampler& heightSampler) {
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

    const std::uint64_t samplesPerAxis = std::uint64_t{quadCount} + 1;
    constexpr std::uint64_t MAX_BUFFER_COUNT = std::numeric_limits<std::uint32_t>::max();
    if (samplesPerAxis > MAX_BUFFER_COUNT / samplesPerAxis ||
            std::uint64_t{quadCount} > MAX_BUFFER_COUNT / quadCount ||
            std::uint64_t{quadCount} * quadCount > MAX_BUFFER_COUNT / 6) {
        throw std::overflow_error{"planet patch mesh exceeds supported buffer counts"};
    }

    const std::uint64_t vertexCount = samplesPerAxis * samplesPerAxis;
    const std::uint64_t indexCount = std::uint64_t{quadCount} * quadCount * 6;
    if (vertexCount > std::numeric_limits<std::size_t>::max() ||
            indexCount > std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error{"planet patch mesh exceeds supported buffer counts"};
    }

    PlanetPatchMeshData result;
    result.id = id;
    result.vertices.reserve(static_cast<std::size_t>(vertexCount));
    result.indices.reserve(static_cast<std::size_t>(indexCount));

    for (std::uint32_t localY = 0; localY <= quadCount; ++localY) {
        for (std::uint32_t localX = 0; localX <= quadCount; ++localX) {
            const wgen::PlanetSurfaceSample surface =
                wgen::patchSurfaceSample(id, quadCount, localX, localY);
            const float height = heightSampler(surface);
            if (!std::isfinite(height)) {
                throw std::invalid_argument{"planet patch mesh height must be finite"};
            }
            result.vertices.push_back({
                glm::vec3{surface.direction} * (height + planetRadius) / planetRadius,
                height,
            });
        }
    }

    const std::uint32_t rowSize = quadCount + 1;
    for (std::uint32_t y = 0; y < quadCount; ++y) {
        for (std::uint32_t x = 0; x < quadCount; ++x) {
            const std::uint32_t topLeft = y * rowSize + x;
            const std::uint32_t topRight = topLeft + 1;
            const std::uint32_t bottomLeft = (y + 1) * rowSize + x;
            const std::uint32_t bottomRight = bottomLeft + 1;
            result.indices.insert(
                result.indices.end(),
                {topLeft, topRight, bottomRight, topLeft, bottomRight, bottomLeft});
        }
    }

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
    if (level > MAX_FIXED_PLANET_PATCH_LEVEL) {
        throw std::invalid_argument{"fixed planet patch level exceeds the debug maximum"};
    }
    if (!std::isfinite(planetRadius) || planetRadius <= 0.0F) {
        throw std::invalid_argument{"fixed planet patches require a finite positive radius"};
    }
    if (!heightSampler) {
        throw std::invalid_argument{"fixed planet patches require a height sampler"};
    }

    const std::uint32_t count = wgen::patchesPerAxis(level);
    std::vector<PlanetPatchMeshData> result;
    result.reserve(wgen::FACES.size() * static_cast<std::size_t>(count) * count);

    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::uint32_t y = 0; y < count; ++y) {
            for (std::uint32_t x = 0; x < count; ++x) {
                result.push_back(buildPlanetPatchMesh(
                    {face, level, x, y},
                    PLANET_PATCH_QUADS,
                    planetRadius,
                    heightSampler));
            }
        }
    }
    return result;
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
                vertices.push_back({
                    cubeSphere.pointUnitDir(face, x, y) * (height + radius) / radius,
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
