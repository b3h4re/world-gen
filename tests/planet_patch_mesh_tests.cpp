#include "terrain/planet/planet_patch_mesh.hpp"

#include "helpers.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace {

void expectVectorNear(const glm::vec3& actual, const glm::vec3& expected, std::string_view message) {
    wgen::tests::require(glm::length(actual - expected) <= 0.00001F, message);
}

void testBilinearDenseSampling() {
    wgen::CubeSphere<float> source{10.0F, 2, 0.0F};
    source.at(wgen::CubeSphereFace::Top, 0, 0) = 0.0F;
    source.at(wgen::CubeSphereFace::Top, 1, 0) = 2.0F;
    source.at(wgen::CubeSphereFace::Top, 0, 1) = 4.0F;
    source.at(wgen::CubeSphereFace::Top, 1, 1) = 6.0F;

    wgen::tests::expectNear(
        lve::sampleCubeSphereBilinear(source, {wgen::CubeSphereFace::Top, -1.0, -1.0, {}}),
        0.0F,
        0.00001F,
        "dense sampler should preserve the lower source texel");
    wgen::tests::expectNear(
        lve::sampleCubeSphereBilinear(source, {wgen::CubeSphereFace::Top, 1.0, 1.0, {}}),
        6.0F,
        0.00001F,
        "dense sampler should preserve the upper source texel");
    wgen::tests::expectNear(
        lve::sampleCubeSphereBilinear(source, {wgen::CubeSphereFace::Top, 0.0, 0.0, {}}),
        3.0F,
        0.00001F,
        "dense sampler should bilinearly interpolate the cell center");
}

void testDenseSamplingPreservesTexels() {
    constexpr std::size_t RESOLUTION = 5;
    wgen::CubeSphere<float> source{25.0F, RESOLUTION, 0.0F};
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < RESOLUTION; ++y) {
            for (std::size_t x = 0; x < RESOLUTION; ++x) {
                source.at(face, x, y) = static_cast<float>(100 * wgen::faceID(face) + 10 * y + x);
                const double u = -1.0 + 2.0 * static_cast<double>(x) / (RESOLUTION - 1);
                const double v = -1.0 + 2.0 * static_cast<double>(y) / (RESOLUTION - 1);
                wgen::tests::expectNear(
                    lve::sampleCubeSphereBilinear(source, {face, u, v, {}}),
                    source.at(face, x, y),
                    0.00001F,
                    "dense sampler should preserve every source texel");
            }
        }
    }
}

void testPatchMeshCountsAndWinding() {
    constexpr std::uint32_t QUADS = 4;
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        std::size_t sampleCalls = 0;
        const lve::PlanetPatchMeshData mesh = lve::buildPlanetPatchMesh(
            {face, 0, 0, 0},
            QUADS,
            100.0F,
            [&sampleCalls](const wgen::PlanetSurfaceSample&) {
                ++sampleCalls;
                return 0.0F;
            });

        wgen::tests::require(mesh.vertices.size() == 25, "patch mesh vertex count is wrong");
        wgen::tests::require(mesh.indices.size() == 96, "patch mesh index count is wrong");
        wgen::tests::require(sampleCalls == mesh.vertices.size(), "patch mesh should sample each vertex once");

        for (const std::uint32_t index : mesh.indices) {
            wgen::tests::require(index < mesh.vertices.size(), "patch mesh index is out of range");
        }
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
            const glm::vec3 a = mesh.vertices[mesh.indices[i]].position;
            const glm::vec3 b = mesh.vertices[mesh.indices[i + 1]].position;
            const glm::vec3 c = mesh.vertices[mesh.indices[i + 2]].position;
            const glm::vec3 normal = glm::cross(b - a, c - a);
            wgen::tests::require(
                glm::dot(normal, a + b + c) > 0.0F,
                "patch mesh triangle should wind outward");
        }
    }
}

void testRootPatchesMatchDenseMesh() {
    constexpr std::size_t RESOLUTION = lve::PLANET_PATCH_QUADS + 1;
    wgen::CubeSphere<float> source{100.0F, RESOLUTION, 0.0F};
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < RESOLUTION; ++y) {
            for (std::size_t x = 0; x < RESOLUTION; ++x) {
                source.at(face, x, y) = 0.001F * static_cast<float>(1000 * wgen::faceID(face) + y * RESOLUTION + x);
            }
        }
    }

    std::vector<lve::Vertex3d> denseVertices;
    std::vector<std::uint32_t> denseIndices;
    lve::appendCubeSphereMesh(source, denseVertices, denseIndices);
    const std::vector<lve::PlanetPatchMeshData> patches =
        lve::buildFixedLevelPlanetPatchMeshes(source, 0);

    const std::size_t verticesPerFace = RESOLUTION * RESOLUTION;
    const std::size_t indicesPerFace = lve::PLANET_PATCH_QUADS * lve::PLANET_PATCH_QUADS * 6;
    wgen::tests::require(patches.size() == wgen::FACES.size(), "root patch count is wrong");
    for (std::size_t faceIndex = 0; faceIndex < patches.size(); ++faceIndex) {
        const lve::PlanetPatchMeshData& patch = patches[faceIndex];
        wgen::tests::require(patch.id.face == wgen::FACES[faceIndex], "root patch face order is wrong");
        for (std::size_t i = 0; i < verticesPerFace; ++i) {
            const lve::Vertex3d& expected = denseVertices[faceIndex * verticesPerFace + i];
            wgen::tests::expectNear(patch.vertices[i].height, expected.height, 0.00001F, "root height changed");
            expectVectorNear(patch.vertices[i].position, expected.position, "root position changed");
        }
        for (std::size_t i = 0; i < indicesPerFace; ++i) {
            const std::uint32_t expected = denseIndices[faceIndex * indicesPerFace + i] -
                static_cast<std::uint32_t>(faceIndex * verticesPerFace);
            wgen::tests::require(patch.indices[i] == expected, "root patch indices changed");
        }
    }
}

std::uint32_t edgeVertexIndex(
        wgen::PlanetPatchEdge edge,
        std::uint32_t quadCount,
        std::uint32_t edgeSample) {
    const std::uint32_t row = quadCount + 1;
    switch (edge) {
        case wgen::PlanetPatchEdge::UMin: return edgeSample * row;
        case wgen::PlanetPatchEdge::UMax: return edgeSample * row + quadCount;
        case wgen::PlanetPatchEdge::VMin: return edgeSample;
        case wgen::PlanetPatchEdge::VMax: return quadCount * row + edgeSample;
    }
    throw std::invalid_argument{"test edge is invalid"};
}

void testPatchMeshSeams() {
    wgen::CubeSphere<float> source{100.0F, 17, 0.0F};
    const std::vector<lve::PlanetPatchMeshData> meshes =
        lve::buildFixedLevelPlanetPatchMeshes(source, 2);
    std::unordered_map<wgen::PlanetPatchId, const lve::PlanetPatchMeshData*, wgen::PlanetPatchIdHash> byId;
    for (const lve::PlanetPatchMeshData& mesh : meshes) {
        byId.emplace(mesh.id, &mesh);
    }

    constexpr std::array EDGES{
        wgen::PlanetPatchEdge::UMin,
        wgen::PlanetPatchEdge::UMax,
        wgen::PlanetPatchEdge::VMin,
        wgen::PlanetPatchEdge::VMax,
    };
    for (const lve::PlanetPatchMeshData& mesh : meshes) {
        for (const wgen::PlanetPatchEdge edge : EDGES) {
            const wgen::PlanetPatchNeighbor neighbor = wgen::sameLevelNeighbor(mesh.id, edge);
            const lve::PlanetPatchMeshData& target = *byId.at(neighbor.id);
            for (std::uint32_t sample = 0; sample <= lve::PLANET_PATCH_QUADS; ++sample) {
                const std::uint32_t targetSample = neighbor.edgeCoordinateReversed
                    ? lve::PLANET_PATCH_QUADS - sample
                    : sample;
                const glm::vec3 sourcePosition = mesh.vertices[
                    edgeVertexIndex(edge, lve::PLANET_PATCH_QUADS, sample)].position;
                const glm::vec3 targetPosition = target.vertices[
                    edgeVertexIndex(neighbor.touchingEdge, lve::PLANET_PATCH_QUADS, targetSample)].position;
                expectVectorNear(sourcePosition, targetPosition, "adjacent patch positions should match");
            }
        }
    }
}

void testFixedLevelPatchCountsAndOrder() {
    wgen::CubeSphere<float> source{100.0F, 2, 0.0F};
    constexpr std::array<std::size_t, 4> EXPECTED_COUNTS{6, 24, 96, 384};
    for (std::uint8_t level = 0; level <= lve::MAX_FIXED_PLANET_PATCH_LEVEL; ++level) {
        const std::vector<lve::PlanetPatchMeshData> meshes =
            lve::buildFixedLevelPlanetPatchMeshes(source, level);
        wgen::tests::require(meshes.size() == EXPECTED_COUNTS[level], "fixed-level patch count is wrong");
        for (const lve::PlanetPatchMeshData& mesh : meshes) {
            wgen::tests::require(
                mesh.vertices.size() == (lve::PLANET_PATCH_QUADS + 1) * (lve::PLANET_PATCH_QUADS + 1),
                "fixed-level patch vertex count is wrong");
            wgen::tests::require(
                mesh.indices.size() == lve::PLANET_PATCH_QUADS * lve::PLANET_PATCH_QUADS * 6,
                "fixed-level patch index count is wrong");
        }

        std::size_t index = 0;
        const std::uint32_t count = wgen::patchesPerAxis(level);
        for (const wgen::CubeSphereFace face : wgen::FACES) {
            for (std::uint32_t y = 0; y < count; ++y) {
                for (std::uint32_t x = 0; x < count; ++x) {
                    wgen::tests::require(
                        meshes[index++].id == wgen::PlanetPatchId{face, level, x, y},
                        "fixed-level patch ordering is wrong");
                }
            }
        }
    }
}

void testGenericFixedLevelSampler() {
    std::size_t sampleCount = 0;
    const std::vector<lve::PlanetPatchMeshData> meshes =
        lve::buildFixedLevelPlanetPatchMeshes(
            250.0F,
            1,
            [&sampleCount](const wgen::PlanetSurfaceSample& surface) {
                ++sampleCount;
                return static_cast<float>(surface.direction.z);
            });

    const std::size_t samplesPerPatch =
        (lve::PLANET_PATCH_QUADS + 1) * (lve::PLANET_PATCH_QUADS + 1);
    wgen::tests::require(meshes.size() == 24, "generic fixed-level patch count is wrong");
    wgen::tests::require(
        sampleCount == meshes.size() * samplesPerPatch,
        "generic fixed-level builder should sample every patch vertex exactly once");
    for (const lve::PlanetPatchMeshData& mesh : meshes) {
        for (const lve::Vertex3d& vertex : mesh.vertices) {
            const float expectedLength = (250.0F + vertex.height) / 250.0F;
            wgen::tests::expectNear(
                glm::length(vertex.position),
                expectedLength,
                0.00001F,
                "generic fixed-level sampler should preserve radius scaling");
        }
    }
}

void testPatchMeshValidation() {
    const lve::PlanetHeightSampler zeroSampler = [](const wgen::PlanetSurfaceSample&) { return 0.0F; };
    wgen::tests::requireThrows<std::invalid_argument>(
        [&zeroSampler] { lve::buildPlanetPatchMesh({wgen::CubeSphereFace::Top, 0, 0, 0}, 0, 1.0F, zeroSampler); },
        "patch mesh should reject zero quads");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&zeroSampler] { lve::buildPlanetPatchMesh({wgen::CubeSphereFace::Top, 0, 0, 0}, 1, 0.0F, zeroSampler); },
        "patch mesh should reject zero radius");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { lve::buildPlanetPatchMesh({wgen::CubeSphereFace::Top, 0, 0, 0}, 1, 1.0F, {}); },
        "patch mesh should reject an empty sampler");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&zeroSampler] {
            lve::buildPlanetPatchMesh(
                {wgen::CubeSphereFace::Top, 1, 2, 0},
                1,
                1.0F,
                zeroSampler);
        },
        "patch mesh should reject an invalid patch ID");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            lve::buildPlanetPatchMesh(
                {wgen::CubeSphereFace::Top, 0, 0, 0},
                1,
                1.0F,
                [](const wgen::PlanetSurfaceSample&) {
                    return std::numeric_limits<float>::quiet_NaN();
                });
        },
        "patch mesh should reject non-finite heights");
    wgen::tests::requireThrows<std::overflow_error>(
        [&zeroSampler] {
            lve::buildPlanetPatchMesh(
                {wgen::CubeSphereFace::Top, 0, 0, 0},
                std::numeric_limits<std::uint32_t>::max(),
                1.0F,
                zeroSampler);
        },
        "patch mesh should reject overflowing counts");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { lve::buildFixedLevelPlanetPatchMeshes(wgen::CubeSphere<float>{10.0F, 2, 0.0F}, 4); },
        "fixed patch builder should reject a level above three");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            lve::buildFixedLevelPlanetPatchMeshes(
                0.0F,
                0,
                [](const wgen::PlanetSurfaceSample&) { return 0.0F; });
        },
        "fixed patch builder should reject zero radius");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { lve::buildFixedLevelPlanetPatchMeshes(100.0F, 0, {}); },
        "fixed patch builder should reject an empty sampler");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            lve::sampleCubeSphereBilinear(
                wgen::CubeSphere<float>{10.0F, 2, 0.0F},
                {wgen::CubeSphereFace::Top, 2.0, 0.0, {}});
        },
        "dense sampler should reject out-of-range UV");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("bilinear dense sampling", testBilinearDenseSampling);
        wgen::tests::runTest("dense sampling preserves texels", testDenseSamplingPreservesTexels);
        wgen::tests::runTest("patch mesh counts and winding", testPatchMeshCountsAndWinding);
        wgen::tests::runTest("root patches match dense mesh", testRootPatchesMatchDenseMesh);
        wgen::tests::runTest("patch mesh seams", testPatchMeshSeams);
        wgen::tests::runTest("fixed-level patch counts and order", testFixedLevelPatchCountsAndOrder);
        wgen::tests::runTest("generic fixed-level sampler", testGenericFixedLevelSampler);
        wgen::tests::runTest("patch mesh validation", testPatchMeshValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
