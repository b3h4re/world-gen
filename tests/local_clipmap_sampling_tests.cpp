#include "terrain/planet/local_clipmap_sampling.hpp"

#include "helpers.hpp"
#include "terrain/generators/3d/generator_spec.hpp"
#include "terrain/planet/cube_sphere.hpp"
#include "terrain/planet/planet_patch.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

constexpr float PLANET_RADIUS = 6'371'000.0F;

wgen::Generator3dSpec makeTerrainSpec(std::size_t octave = 1) {
    return {
        .kind = wgen::Generator3dKind::PerlinNoise,
        .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.4F},
        .scale = 350.0F,
        .bias = 25.0F,
        .octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = octave,
            .lacunarity = 2.0F,
            .persistance = 0.5F,
        },
        .terrainDetail = wgen::Generator3dTerrainDetailSpec{
            .firstFullyVisibleLevel = 0,
        },
    };
}

void requireVectorNear(
        const glm::dvec3& actual,
        const glm::dvec3& expected,
        double tolerance,
        std::string_view message) {
    wgen::tests::require(glm::length(actual - expected) <= tolerance, message);
}

void testLocalPlanetFrameAndSphericalMapping() {
    constexpr double radius = 1'000.0;
    const glm::dvec3 anchorDirection = glm::normalize(
        glm::dvec3{0.3, -0.4, 0.8});
    const wgen::LocalPlanetFrame frame = wgen::makeLocalPlanetFrame(
        anchorDirection,
        radius,
        7);
    wgen::validateLocalPlanetFrame(frame);

    requireVectorNear(
        frame.anchorDirection,
        anchorDirection,
        0.000000000001,
        "local frame should retain its anchor direction");
    requireVectorNear(
        frame.globalAnchorPositionMeters,
        anchorDirection * radius,
        0.000000001,
        "local frame global anchor should lie on the reference sphere");
    wgen::tests::require(
        frame.revision == 7 &&
            std::abs(glm::dot(frame.east, frame.north)) <= 0.000000000001 &&
            std::abs(glm::dot(frame.east, frame.up)) <= 0.000000000001,
        "local frame should preserve a versioned orthonormal basis");

    requireVectorNear(
        wgen::localPlanetSurfaceDirection(frame, {}),
        anchorDirection,
        0.000000000001,
        "zero tangent offset should map to the anchor");
    const double distance = radius * std::numbers::pi / 6.0;
    const glm::dvec3 expectedDirection =
        frame.up * std::cos(std::numbers::pi / 6.0) +
        frame.east * std::sin(std::numbers::pi / 6.0);
    requireVectorNear(
        wgen::localPlanetSurfaceDirection(frame, {distance, 0.0}),
        expectedDirection,
        0.000000000001,
        "tangent offsets should use the spherical exponential mapping");

    requireVectorNear(
        wgen::localPlanetTangentFlatPosition(frame, {12.0, -8.0}, 3.0),
        frame.globalAnchorPositionMeters + frame.east * 12.0 -
            frame.north * 8.0 + frame.up * 3.0,
        0.000000001,
        "flat local positions should use the stored tangent basis");
}

void testClipmapAndQuadtreeSampleEquivalence() {
    const wgen::TerrainFieldSnapshot field =
        wgen::buildTerrainFieldSnapshot({makeTerrainSpec()}, 41, PLANET_RADIUS);
    constexpr std::uint8_t patchLevel = 4;
    constexpr std::uint32_t patchQuads = 32;
    const wgen::PlanetSurfaceSample patchSample = wgen::patchSurfaceSample(
        {wgen::CubeSphereFace::Top, patchLevel, 7, 9},
        patchQuads,
        13,
        21);
    const double matchingSpacing =
        field->detailPolicy().cubeFacePatchSampleSpacing(
            patchLevel,
            patchQuads);

    wgen::LocalClipmapConfig config{};
    config.levelCount = 1;
    config.finestCellSpacingMeters = matchingSpacing;
    const wgen::LocalPlanetFrame frame = wgen::makeLocalPlanetFrame(
        patchSample.direction,
        field->radius(),
        11);
    const wgen::LocalClipmapCacheOrigin origin{{}, 11, 0, {}};
    const glm::dvec3 camera =
        patchSample.direction * (field->radius() + 1'000.0);
    const wgen::LocalClipmapSurfaceSample localSample =
        wgen::sampleLocalClipmapPoint(
            frame,
            field,
            config,
            origin,
            {},
            camera);

    const wgen::TerrainDetailLevel patchDetail =
        field->detailPolicy().detailForCubeFacePatch(
            patchLevel,
            patchQuads);
    wgen::tests::require(
        localSample.terrainDetail == patchDetail,
        "equivalent physical spacing should request equal terrain detail");
    wgen::tests::expectNear(
        localSample.heightMeters,
        field->sample(patchSample, patchDetail),
        0.0F,
        "clipmap and quadtree queries should return the same physical height");
    requireVectorNear(
        localSample.globalDirection,
        patchSample.direction,
        0.000000000001,
        "the clipmap anchor sample should retain the quadtree direction");
    requireVectorNear(
        localSample.curvedCameraRelativePositionMeters,
        patchSample.direction *
                (static_cast<double>(field->radius()) +
                    static_cast<double>(localSample.heightMeters)) -
            camera,
        0.000000001,
        "curved samples should be camera-relative physical positions");
}

void testCurvedAndFlatPositions() {
    constexpr float radius = 10'000.0F;
    const wgen::TerrainFieldSnapshot field =
        wgen::buildTerrainFieldSnapshot({}, 1, radius);
    wgen::LocalClipmapConfig config{};
    config.samplesPerAxis = 9;
    config.levelCount = 3;
    config.finestCellSpacingMeters = 50.0;
    config.overlapWidthCells = 1;
    config.transitionWidthCells = 1;
    const wgen::LocalPlanetFrame frame = wgen::makeLocalPlanetFrame(
        glm::dvec3{0.0, 0.0, 1.0},
        radius,
        3);
    const glm::dvec3 camera =
        frame.globalAnchorPositionMeters + frame.up * 100.0;
    const wgen::LocalClipmapCacheOrigin middleOrigin{{}, 3, 1, {}};
    const wgen::LocalClipmapSurfaceSample flat =
        wgen::sampleLocalClipmapPoint(
            frame,
            field,
            config,
            middleOrigin,
            {3, 2},
            camera);

    wgen::tests::require(
        flat.curvatureWeight == 0.0 &&
            flat.blendedCameraRelativePositionMeters ==
                flat.tangentFlatCameraRelativePositionMeters,
        "the initial standalone clipmap should default to tangent-flat output");
    wgen::tests::require(
        glm::length(
            flat.curvedCameraRelativePositionMeters -
            flat.tangentFlatCameraRelativePositionMeters) > 0.1,
        "non-zero offsets should retain distinct curved and flat positions");

    const wgen::LocalClipmapSurfaceSample blended =
        wgen::sampleLocalClipmapPoint(
            frame,
            field,
            config,
            middleOrigin,
            {3, 2},
            camera,
            {.curvatureWeight = 0.25});
    requireVectorNear(
        blended.blendedCameraRelativePositionMeters,
        blended.tangentFlatCameraRelativePositionMeters * 0.75 +
            blended.curvedCameraRelativePositionMeters * 0.25,
        0.000000001,
        "curvature weight should blend the two camera-relative positions");

    const wgen::LocalClipmapSamplingConfig forceOuter{
        .forceOutermostRingCurved = true,
    };
    wgen::tests::require(
        wgen::localClipmapCurvatureWeight(forceOuter, 1, 3) == 0.0 &&
            wgen::localClipmapCurvatureWeight(forceOuter, 2, 3) == 1.0,
        "the outermost ring should be independently forceable to full curvature");
    const wgen::LocalClipmapCacheOrigin outerOrigin{{}, 3, 2, {}};
    const wgen::LocalClipmapSurfaceSample curvedOuter =
        wgen::sampleLocalClipmapPoint(
            frame,
            field,
            config,
            outerOrigin,
            {1, 1},
            camera,
            forceOuter);
    wgen::tests::require(
        curvedOuter.curvatureWeight == 1.0 &&
            curvedOuter.blendedCameraRelativePositionMeters ==
                curvedOuter.curvedCameraRelativePositionMeters,
        "forced outer-ring samples should select the curved position");
}

std::vector<wgen::LocalClipmapGridCoordinate> verticalGridLine() {
    std::vector<wgen::LocalClipmapGridCoordinate> coordinates;
    for (std::int64_t y = -4; y <= 4; ++y) {
        coordinates.push_back({0, y});
    }
    return coordinates;
}

void requireSamplesIndependentOfFace(
        const wgen::TerrainFieldSnapshot& field,
        const std::vector<wgen::LocalClipmapSurfaceSample>& samples) {
    for (const wgen::LocalClipmapSurfaceSample& sample : samples) {
        wgen::tests::require(
            std::abs(glm::length(sample.globalDirection) - 1.0) <=
                0.000000000001,
            "local clipmap direction should remain unit length");
        wgen::tests::expectNear(
            sample.heightMeters,
            field->sample(sample.globalDirection, sample.terrainDetail),
            0.0F,
            "local clipmap height should use its global direction");
        for (const wgen::CubeSphereFace face : wgen::FACES) {
            wgen::tests::expectNear(
                sample.heightMeters,
                field->sample(
                    {face, 0.0, 0.0, sample.globalDirection},
                    sample.terrainDetail),
                0.0F,
                "terrain height should not depend on cube-face renderer ownership");
        }
    }
}

void testSamplingAcrossCubeEdgesAndPoles() {
    const wgen::TerrainFieldSnapshot field =
        wgen::buildTerrainFieldSnapshot({makeTerrainSpec(2)}, 73, PLANET_RADIUS);
    wgen::LocalClipmapConfig config{};
    config.samplesPerAxis = 9;
    config.levelCount = 2;
    config.finestCellSpacingMeters =
        static_cast<double>(PLANET_RADIUS) * 0.04;
    config.overlapWidthCells = 1;
    config.transitionWidthCells = 1;
    const std::vector<wgen::LocalClipmapGridCoordinate> coordinates =
        verticalGridLine();

    const glm::dvec3 edgeDirection = wgen::cubeSphereDirection(
        wgen::CubeSphereFace::Top,
        1.0,
        0.0);
    const wgen::LocalPlanetFrame edgeFrame = wgen::makeLocalPlanetFrame(
        edgeDirection,
        field->radius(),
        21);
    const std::vector<wgen::LocalClipmapSurfaceSample> edgeSamples =
        wgen::sampleLocalClipmapPoints(
            edgeFrame,
            field,
            config,
            {{}, 21, 0, {}},
            coordinates,
            edgeFrame.globalAnchorPositionMeters + edgeFrame.up * 1'000.0);
    requireSamplesIndependentOfFace(field, edgeSamples);

    std::set<wgen::CubeSphereFace> facesUnderGrid;
    for (const wgen::LocalClipmapSurfaceSample& sample : edgeSamples) {
        facesUnderGrid.insert(
            wgen::directionToCubeSphereFaceUv(sample.globalDirection).face);
    }
    wgen::tests::require(
        facesUnderGrid.size() >= 2,
        "a clipmap centered on a cube edge should sample across face ownership");

    const wgen::LocalPlanetFrame poleFrame = wgen::makeLocalPlanetFrame(
        glm::dvec3{0.0, 0.0, 1.0},
        field->radius(),
        22);
    const std::array poleCoordinates{
        wgen::LocalClipmapGridCoordinate{-4, -4},
        wgen::LocalClipmapGridCoordinate{0, 0},
        wgen::LocalClipmapGridCoordinate{4, -4},
        wgen::LocalClipmapGridCoordinate{-4, 4},
        wgen::LocalClipmapGridCoordinate{4, 4},
    };
    const std::vector<wgen::LocalClipmapSurfaceSample> poleSamples =
        wgen::sampleLocalClipmapPoints(
            poleFrame,
            field,
            config,
            {{}, 22, 0, {}},
            poleCoordinates,
            poleFrame.globalAnchorPositionMeters + poleFrame.up * 1'000.0);
    requireSamplesIndependentOfFace(field, poleSamples);
}

void testBatchUsesOneImmutableSnapshot() {
    const wgen::TerrainFieldSnapshot first =
        wgen::buildTerrainFieldSnapshot({makeTerrainSpec()}, 101, PLANET_RADIUS);
    const wgen::TerrainFieldSnapshot replacement =
        wgen::buildTerrainFieldSnapshot({makeTerrainSpec()}, 102, PLANET_RADIUS);
    wgen::LocalClipmapConfig config{};
    config.samplesPerAxis = 9;
    config.levelCount = 2;
    config.finestCellSpacingMeters = 1'000.0;
    config.overlapWidthCells = 1;
    config.transitionWidthCells = 1;
    const wgen::LocalPlanetFrame frame = wgen::makeLocalPlanetFrame(
        glm::normalize(glm::dvec3{0.2, -0.7, 0.5}),
        first->radius(),
        31);
    const std::array coordinates{
        wgen::LocalClipmapGridCoordinate{-2, 3},
        wgen::LocalClipmapGridCoordinate{0, 0},
        wgen::LocalClipmapGridCoordinate{4, -1},
    };
    const std::vector<wgen::LocalClipmapSurfaceSample> samples =
        wgen::sampleLocalClipmapPoints(
            frame,
            first,
            config,
            {{}, 31, 0, {}},
            coordinates,
            frame.globalAnchorPositionMeters + frame.up * 500.0);

    bool replacementDiffers = false;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const wgen::LocalClipmapSurfaceSample& sample = samples[index];
        wgen::tests::require(
            sample.gridCoordinate == coordinates[index],
            "clipmap batch should preserve deterministic input order");
        wgen::tests::expectNear(
            sample.heightMeters,
            first->sample(sample.globalDirection, sample.terrainDetail),
            0.0F,
            "every batch point should use the requested immutable snapshot");
        replacementDiffers = replacementDiffers ||
            std::abs(
                sample.heightMeters -
                replacement->sample(
                    sample.globalDirection,
                    sample.terrainDetail)) > 0.0001F;
    }
    wgen::tests::require(
        replacementDiffers,
        "test snapshots should represent distinct terrain epochs");
}

void testValidation() {
    const wgen::LocalPlanetFrame validFrame = wgen::makeLocalPlanetFrame(
        glm::dvec3{0.0, 0.0, 1.0},
        100.0,
        1);
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::makeLocalPlanetFrame(
                glm::dvec3{0.0, 0.0, 1.0},
                100.0,
                0);
        },
        "local frames should reject a zero revision");
    wgen::LocalPlanetFrame leftHanded = validFrame;
    leftHanded.east = -leftHanded.east;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&leftHanded] { wgen::validateLocalPlanetFrame(leftHanded); },
        "local frames should reject a left-handed basis");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&validFrame] {
            wgen::localPlanetSurfaceDirection(
                validFrame,
                {100.0 * std::numbers::pi, 0.0});
        },
        "local spherical mapping should reject the ambiguous antipode");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::validateLocalClipmapSamplingConfig({
                .curvatureWeight =
                    std::numeric_limits<double>::quiet_NaN(),
            });
        },
        "clipmap sampling should reject a non-finite curvature weight");

    const wgen::TerrainFieldSnapshot field =
        wgen::buildTerrainFieldSnapshot({}, 1, 100.0F);
    wgen::LocalClipmapConfig config{};
    config.levelCount = 1;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&validFrame, &config] {
            wgen::sampleLocalClipmapPoint(
                validFrame,
                {},
                config,
                {{}, 1, 0, {}},
                {},
                {});
        },
        "clipmap sampling should require a terrain snapshot");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&validFrame, &field, &config] {
            wgen::sampleLocalClipmapPoint(
                validFrame,
                field,
                config,
                {{}, 2, 0, {}},
                {},
                {});
        },
        "clipmap sampling should reject stale frame coordinates");
    const wgen::LocalPlanetFrame wrongRadius = wgen::makeLocalPlanetFrame(
        glm::dvec3{0.0, 0.0, 1.0},
        101.0,
        1);
    wgen::tests::requireThrows<std::invalid_argument>(
        [&wrongRadius, &field, &config] {
            wgen::sampleLocalClipmapPoint(
                wrongRadius,
                field,
                config,
                {{}, 1, 0, {}},
                {},
                {});
        },
        "clipmap sampling should reject mismatched planet radii");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest(
            "local planet frame and spherical mapping",
            testLocalPlanetFrameAndSphericalMapping);
        wgen::tests::runTest(
            "clipmap and quadtree sample equivalence",
            testClipmapAndQuadtreeSampleEquivalence);
        wgen::tests::runTest(
            "curved and flat positions",
            testCurvedAndFlatPositions);
        wgen::tests::runTest(
            "sampling across cube edges and poles",
            testSamplingAcrossCubeEdgesAndPoles);
        wgen::tests::runTest(
            "immutable snapshot batch",
            testBatchUsesOneImmutableSnapshot);
        wgen::tests::runTest("validation", testValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
