#include "terrain/planet/terrain_field.hpp"

#include "helpers.hpp"
#include "terrain/generators/3d/generator_factory.hpp"
#include "terrain/utils/hash_random.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

wgen::Generator3dSpec makeSpec(
        std::size_t octave,
        std::uint8_t firstVisibleLod = 0,
        wgen::TerrainComputeMethod computeMethod = wgen::TerrainComputeMethod::Cpu) {
    return {
        .kind = wgen::Generator3dKind::PerlinNoise,
        .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
        .scale = 1.0F,
        .computeMethod = computeMethod,
        .octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = octave,
            .lacunarity = 2.0F,
            .persistance = 0.5F,
        },
        .firstVisibleLod = firstVisibleLod,
    };
}

wgen::PlanetSurfaceSample makeSurface(glm::vec3 direction) {
    return {wgen::CubeSphereFace::Top, 0.0, 0.0, glm::dvec3{direction}};
}

void expectCubeSphereNear(
        const wgen::CubeSphere<float>& actual,
        const wgen::CubeSphere<float>& expected,
        std::string_view message) {
    wgen::tests::require(actual.resolution() == expected.resolution(), message);
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < actual.resolution(); ++y) {
            for (std::size_t x = 0; x < actual.resolution(); ++x) {
                wgen::tests::expectNear(actual.at(face, x, y), expected.at(face, x, y), 0.00002F, message);
            }
        }
    }
}

void testCalibrationMatchesDenseCpuPipeline() {
    const wgen::Generator3dPipelineSpec specs{makeSpec(0), makeSpec(1)};
    const wgen::SeedType seed = 17;
    const auto field = wgen::buildTerrainFieldSnapshot(specs, seed, 125.0F);
    const auto pipeline = wgen::makePipeline3d(specs, seed);
    wgen::CubeSphere<float> expected = pipeline->generateCubeSphere(wgen::TERRAIN_CALIBRATION_RESOLUTION);
    expected.setRadius(125.0F);
    expected.normalize();

    const wgen::CubeSphere<float> actual = field->generateCubeSphere(
        wgen::TERRAIN_CALIBRATION_RESOLUTION,
        wgen::MAX_PLANET_PATCH_LEVEL);
    expectCubeSphereNear(actual, expected, "terrain field should match calibrated dense CPU output");

    float minimum = std::numeric_limits<float>::infinity();
    float maximum = -std::numeric_limits<float>::infinity();
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < actual.resolution(); ++y) {
            for (std::size_t x = 0; x < actual.resolution(); ++x) {
                minimum = std::min(minimum, actual.at(face, x, y));
                maximum = std::max(maximum, actual.at(face, x, y));
            }
        }
    }
    wgen::tests::expectNear(minimum, -1.0F, 0.00002F, "calibrated minimum should be negative one");
    wgen::tests::expectNear(maximum, 1.0F, 0.00002F, "calibrated maximum should be positive one");
}

void testDetailBandsUseFrozenCalibration() {
    wgen::Generator3dSpec broad = makeSpec(0, 0);
    broad.scale = 0.75F;
    wgen::Generator3dSpec detail = makeSpec(1, 2);
    detail.scale = 1.25F;
    const wgen::SeedType seed = 23;
    const auto field = wgen::buildTerrainFieldSnapshot({broad, detail}, seed, 100.0F);
    const wgen::TerrainHeightCalibration calibration = field->calibration();
    const glm::vec3 direction = glm::normalize(glm::vec3{0.3F, -0.7F, 0.2F});

    const auto broadGenerator = wgen::makePipelineGenerator3d(broad, seed);
    const auto detailGenerator = wgen::makePipelineGenerator3d(detail, wgen::hashSeed(seed));
    const float broadRaw = broadGenerator->noise(direction) *
        broad.scale * wgen::generator3dOctaveAmplitude(broad);
    const float detailRaw = detailGenerator->noise(direction) *
        detail.scale * wgen::generator3dOctaveAmplitude(detail);

    wgen::tests::expectNear(
        field->sample(makeSurface(direction), 0),
        calibration.apply(broadRaw),
        0.00001F,
        "level zero should include only the broad contributor");
    wgen::tests::expectNear(
        field->sample(makeSurface(direction), 1),
        calibration.apply(broadRaw),
        0.00001F,
        "detail should remain hidden below its first visible level");
    wgen::tests::expectNear(
        field->sample(makeSurface(direction), 2),
        calibration.apply(broadRaw + detailRaw),
        0.00001F,
        "detail should appear at its first visible level");

    const wgen::TerrainHeightCalibration afterSampling = field->calibration();
    wgen::tests::require(
        calibration.rawMinimum == afterSampling.rawMinimum &&
            calibration.rawMaximum == afterSampling.rawMaximum &&
            calibration.scale == afterSampling.scale &&
            calibration.bias == afterSampling.bias,
        "sampling different detail levels must not recalibrate the field");
}

void testComputeMetadataDoesNotChangeRuntimeSampling() {
    const auto cpu = wgen::buildTerrainFieldSnapshot(
        {makeSpec(0, 0, wgen::TerrainComputeMethod::Cpu)},
        42,
        100.0F);
    const auto gpuMarked = wgen::buildTerrainFieldSnapshot(
        {makeSpec(0, 0, wgen::TerrainComputeMethod::VulkanCompute)},
        42,
        100.0F);

    for (const glm::vec3 direction : {
             glm::normalize(glm::vec3{1.0F, 2.0F, 3.0F}),
             glm::normalize(glm::vec3{-0.5F, 0.3F, 0.8F}),
             glm::normalize(glm::vec3{0.2F, -1.0F, -0.4F})}) {
        wgen::tests::expectNear(
            cpu->sample(makeSurface(direction), 0),
            gpuMarked->sample(makeSurface(direction), 0),
            0.00001F,
            "runtime field sampling should ignore compute metadata");
    }
}

void testDeterminismAndSeedChanges() {
    const wgen::Generator3dPipelineSpec specs{makeSpec(0), makeSpec(1)};
    const auto first = wgen::buildTerrainFieldSnapshot(specs, 10, 100.0F);
    const auto second = wgen::buildTerrainFieldSnapshot(specs, 10, 100.0F);
    const auto changed = wgen::buildTerrainFieldSnapshot(specs, 11, 100.0F);
    bool foundSeedDifference = false;

    for (const glm::vec3 direction : {
             glm::normalize(glm::vec3{1.0F, 0.2F, 0.7F}),
             glm::normalize(glm::vec3{-0.3F, 0.9F, -0.4F}),
             glm::normalize(glm::vec3{0.5F, -0.8F, 0.1F})}) {
        const float firstValue = first->sample(makeSurface(direction), wgen::MAX_PLANET_PATCH_LEVEL);
        wgen::tests::expectNear(
            firstValue,
            second->sample(makeSurface(direction), wgen::MAX_PLANET_PATCH_LEVEL),
            0.00001F,
            "identical terrain snapshots should sample identically");
        foundSeedDifference = foundSeedDifference || std::abs(
            firstValue - changed->sample(makeSurface(direction), wgen::MAX_PLANET_PATCH_LEVEL)) > 0.00001F;
    }
    wgen::tests::require(foundSeedDifference, "changing the terrain seed should change field output");
}

void testEmptyAndConstantFieldsReturnZero() {
    const auto empty = wgen::buildTerrainFieldSnapshot({}, 1, 100.0F);
    wgen::Generator3dSpec constantSpec = makeSpec(0);
    constantSpec.scale = 0.0F;
    const auto constant = wgen::buildTerrainFieldSnapshot({constantSpec}, 1, 100.0F);
    const wgen::PlanetSurfaceSample surface = makeSurface(glm::vec3{0.0F, 0.0F, 1.0F});

    wgen::tests::expectNear(empty->sample(surface, 0), 0.0F, 0.0F, "empty field should return zero");
    wgen::tests::expectNear(constant->sample(surface, 0), 0.0F, 0.0F, "constant field should return zero");
    wgen::tests::expectNear(empty->maximumAbsoluteHeight(), 0.0F, 0.0F, "empty field should have a zero height bound");
    wgen::tests::expectNear(constant->maximumAbsoluteHeight(), 0.0F, 0.0F, "constant field should have a zero height bound");
    wgen::tests::require(constant->calibration().scale == 0.0F, "constant field should have zero calibration scale");
}

void testConservativeHeightBound() {
    wgen::Generator3dSpec broad = makeSpec(0, 0);
    broad.scale = -1.25F;
    wgen::Generator3dSpec detail = makeSpec(2, 3);
    detail.scale = 0.8F;
    const auto field = wgen::buildTerrainFieldSnapshot({broad, detail}, 53, 100.0F);
    const float bound = field->maximumAbsoluteHeight();
    wgen::tests::require(std::isfinite(bound) && bound > 0.0F, "terrain field height bound must be finite and positive");

    constexpr std::size_t resolution = 17;
    constexpr std::uint8_t detailLevels[]{0, 2, 3, wgen::MAX_PLANET_PATCH_LEVEL};
    for (const std::uint8_t detailLevel : detailLevels) {
        const wgen::CubeSphere<float> samples = field->generateCubeSphere(resolution, detailLevel);
        for (const wgen::CubeSphereFace face : wgen::FACES) {
            for (std::size_t y = 0; y < resolution; ++y) {
                for (std::size_t x = 0; x < resolution; ++x) {
                    wgen::tests::require(
                        std::abs(samples.at(face, x, y)) <= bound + 0.0001F,
                        "terrain sample exceeded the conservative field height bound");
                }
            }
        }
    }
}

void testSeamsAndArbitraryDenseResolution() {
    const auto field = wgen::buildTerrainFieldSnapshot({makeSpec(0)}, 31, 100.0F);
    const glm::vec3 sharedDirection = wgen::cubeSphereDirection(wgen::CubeSphereFace::Top, -1.0F, 0.25F);
    const wgen::PlanetSurfaceSample top{
        wgen::CubeSphereFace::Top, -1.0, 0.25, glm::dvec3{sharedDirection}};
    const wgen::PlanetSurfaceSample left{
        wgen::CubeSphereFace::Left, 0.0, 0.0, glm::dvec3{sharedDirection}};
    wgen::tests::expectNear(
        field->sample(top, 0),
        field->sample(left, 0),
        0.0F,
        "equivalent face-edge directions should sample identically");

    const wgen::CubeSphere<float> dense = field->generateCubeSphere(17, 0);
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < dense.resolution(); ++y) {
            for (std::size_t x = 0; x < dense.resolution(); ++x) {
                const float denominator = static_cast<float>(dense.resolution() - 1);
                const float u = -1.0F + 2.0F * static_cast<float>(x) / denominator;
                const float v = -1.0F + 2.0F * static_cast<float>(y) / denominator;
                const glm::vec3 direction = dense.pointUnitDir(face, x, y);
                wgen::tests::expectNear(
                    dense.at(face, x, y),
                    field->sample({face, u, v, glm::dvec3{direction}}, 0),
                    0.00001F,
                    "dense compatibility output should use frozen field calibration");
            }
        }
    }
}

void testValidation() {
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::buildTerrainFieldSnapshot({}, 1, 0.0F); },
        "terrain field should reject zero radius");

    wgen::Generator3dSpec invalidLod = makeSpec(0);
    invalidLod.firstVisibleLod = wgen::MAX_PLANET_PATCH_LEVEL + 1;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&invalidLod] { wgen::buildTerrainFieldSnapshot({invalidLod}, 1, 100.0F); },
        "terrain field should reject an invalid visibility level");

    wgen::Generator3dSpec invalidScale = makeSpec(0);
    invalidScale.scale = std::numeric_limits<float>::infinity();
    wgen::tests::requireThrows<std::invalid_argument>(
        [&invalidScale] { wgen::buildTerrainFieldSnapshot({invalidScale}, 1, 100.0F); },
        "terrain field should reject a non-finite amplitude");

    wgen::Generator3dSpec invalidGenerator = makeSpec(0);
    invalidGenerator.config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = -1.0F};
    wgen::tests::requireThrows<std::invalid_argument>(
        [&invalidGenerator] { wgen::buildTerrainFieldSnapshot({invalidGenerator}, 1, 100.0F); },
        "terrain field should reject invalid generator configuration");

    const auto field = wgen::buildTerrainFieldSnapshot({}, 1, 100.0F);
    wgen::tests::requireThrows<std::invalid_argument>(
        [&field] {
            field->sample(
                makeSurface(glm::vec3{0.0F, 0.0F, 1.0F}),
                wgen::MAX_PLANET_PATCH_LEVEL + 1);
        },
        "terrain field should reject an invalid sample detail level");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&field] { field->generateCubeSphere(1, 0); },
        "terrain field should reject a one-sample dense resolution");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("terrain field dense parity", testCalibrationMatchesDenseCpuPipeline);
        wgen::tests::runTest("terrain field detail bands", testDetailBandsUseFrozenCalibration);
        wgen::tests::runTest("terrain field compute metadata", testComputeMetadataDoesNotChangeRuntimeSampling);
        wgen::tests::runTest("terrain field determinism", testDeterminismAndSeedChanges);
        wgen::tests::runTest("terrain field constant handling", testEmptyAndConstantFieldsReturnZero);
        wgen::tests::runTest("terrain field conservative height bound", testConservativeHeightBound);
        wgen::tests::runTest("terrain field seams and dense output", testSeamsAndArbitraryDenseResolution);
        wgen::tests::runTest("terrain field validation", testValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
