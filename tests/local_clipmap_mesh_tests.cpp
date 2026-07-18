#include "terrain/planet/local_clipmap_mesh.hpp"

#include "helpers.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

wgen::LocalClipmapConfig testConfig() {
    return {
        .samplesPerAxis = 9,
        .levelCount = 2,
        .finestCellSpacingMeters = 10.0,
        .levelScale = 2,
        .overlapWidthCells = 1,
        .transitionWidthCells = 1,
    };
}

void requireNear(
        const glm::dvec3& actual,
        const glm::dvec3& expected,
        double epsilon,
        std::string_view message) {
    wgen::tests::require(
        glm::length(actual - expected) <= epsilon,
        message);
}

void testCurvedPositionsMatchSurfaceContract() {
    const wgen::LocalClipmapConfig config = testConfig();
    const wgen::LocalClipmapTopology topology =
        wgen::buildLocalClipmapTopology(config);
    const wgen::LocalPlanetFrame frame = wgen::makeLocalPlanetFrame(
        glm::dvec3{0.2, 0.9, -0.3},
        10'000.0,
        7);
    const wgen::LocalClipmapCacheOrigin origin{
        {},
        frame.revision,
        0,
        {3, -2},
    };
    const wgen::LocalClipmapUpdateIdentity identity{
        .terrainEpoch = 4,
        .terrainDependencyRevision = 0,
        .localFrameRevision = frame.revision,
        .level = origin.level,
        .snappedGridOrigin = origin.centerSample,
        .requestRevision = 11,
    };
    std::vector<float> heights(topology.vertices.size());
    for (std::size_t index = 0; index < heights.size(); ++index) {
        heights[index] = 5.0F + static_cast<float>(index) * 0.01F;
    }

    const lve::LocalClipmapLevelMeshData mesh =
        lve::buildLocalClipmapLevelMesh(
            frame,
            topology,
            origin,
            identity,
            heights);
    wgen::tests::require(
        mesh.level == 0 && mesh.identity == identity &&
            mesh.vertices.size() == topology.vertices.size(),
        "clipmap mesh should preserve its level, identity, and shared lattice");
    requireNear(
        mesh.globalOrigin,
        frame.anchorDirection,
        1.0e-14,
        "clipmap mesh global origin should remain double precision");

    for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
        const glm::dvec2 localPosition = wgen::localClipmapVertexPosition(
            origin,
            topology.vertices[index],
            config.finestCellSpacingMeters);
        const glm::dvec3 direction = wgen::localPlanetSurfaceDirection(
            frame,
            localPosition);
        const glm::dvec3 expected = wgen::localPlanetCurvedPosition(
            frame,
            direction,
            heights[index]) / frame.planetRadiusMeters;
        const glm::dvec3 reconstructed = mesh.globalOrigin +
            glm::dvec3{mesh.vertices[index].position};
        requireNear(
            reconstructed,
            expected,
            2.0e-7,
            "CPU fallback position should match the shared curved sampler");
        wgen::tests::require(
            mesh.vertices[index].height == heights[index] &&
                std::abs(
                    mesh.vertices[index].parentPosition.x -
                    static_cast<float>(localPosition.x)) < 1.0e-5F &&
                std::abs(
                    mesh.vertices[index].parentPosition.y -
                    static_cast<float>(localPosition.y)) < 1.0e-5F &&
                mesh.vertices[index].parentHeight == heights[index],
            "CPU fallback should retain height and tangent coverage coordinates");
    }
}

void testOutwardWindingAndNormals() {
    const wgen::LocalClipmapConfig config = testConfig();
    const wgen::LocalClipmapTopology topology =
        wgen::buildLocalClipmapTopology(config);
    const wgen::LocalPlanetFrame frame = wgen::makeLocalPlanetFrame(
        glm::dvec3{0.0, 1.0, 0.0},
        5'000.0,
        2);
    const wgen::LocalClipmapCacheOrigin origin{{}, 2, 1, {0, 0}};
    const wgen::LocalClipmapUpdateIdentity identity{
        .terrainEpoch = 1,
        .localFrameRevision = 2,
        .level = 1,
        .snappedGridOrigin = {},
        .requestRevision = 1,
    };
    const std::vector<float> heights(topology.vertices.size(), 0.0F);
    const lve::LocalClipmapLevelMeshData mesh =
        lve::buildLocalClipmapLevelMesh(
            frame,
            topology,
            origin,
            identity,
            heights);

    for (std::size_t index = 0;
         index < topology.ringPattern.indices.size();
         index += 3) {
        const glm::dvec3 a = mesh.globalOrigin + glm::dvec3{
            mesh.vertices[topology.ringPattern.indices[index]].position};
        const glm::dvec3 b = mesh.globalOrigin + glm::dvec3{
            mesh.vertices[topology.ringPattern.indices[index + 1]].position};
        const glm::dvec3 c = mesh.globalOrigin + glm::dvec3{
            mesh.vertices[topology.ringPattern.indices[index + 2]].position};
        const glm::dvec3 normal = glm::cross(b - a, c - a);
        wgen::tests::require(
            std::isfinite(normal.x) && std::isfinite(normal.y) &&
                std::isfinite(normal.z) && glm::length(normal) > 0.0,
            "curved clipmap triangle normal should be finite and non-zero");
        wgen::tests::require(
            glm::dot(normal, glm::normalize(a + b + c)) > 0.0,
            "curved clipmap triangle winding should face away from the planet");
    }
}

void testValidation() {
    const wgen::LocalClipmapConfig config = testConfig();
    const wgen::LocalClipmapTopology topology =
        wgen::buildLocalClipmapTopology(config);
    const wgen::LocalPlanetFrame frame = wgen::makeLocalPlanetFrame(
        glm::dvec3{0.0, 0.0, 1.0},
        1'000.0,
        1);
    const wgen::LocalClipmapCacheOrigin origin{{}, 1, 0, {}};
    const wgen::LocalClipmapUpdateIdentity identity{
        .terrainEpoch = 1,
        .localFrameRevision = 1,
        .level = 0,
        .snappedGridOrigin = {},
        .requestRevision = 1,
    };
    wgen::tests::requireThrows<std::invalid_argument>(
        [&] {
            static_cast<void>(lve::buildLocalClipmapLevelMesh(
                frame,
                topology,
                origin,
                identity,
                std::vector<float>{0.0F}));
        },
        "clipmap mesh should require a complete height lattice");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest(
            "curved positions match the surface contract",
            testCurvedPositionsMatchSurfaceContract);
        wgen::tests::runTest(
            "outward winding and finite normals",
            testOutwardWindingAndNormals);
        wgen::tests::runTest("validation", testValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
