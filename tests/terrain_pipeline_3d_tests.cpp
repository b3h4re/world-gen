#include "terrain/generators/3d/generator_factory.hpp"
#include "terrain/generators/3d/noise/perlin.hpp"
#include "terrain/generators/3d/terrain_pipeline.hpp"
#include "terrain/utils/hash_random.hpp"

#include "helpers.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void expectCubeSphereNear(
        const wgen::CubeSphere<float>& actual,
        const wgen::CubeSphere<float>& expected,
        std::string_view message) {
    wgen::tests::expectMapNear(actual, expected, message);
}

void expectVectorNear(const glm::vec3& actual, const glm::vec3& expected, std::string_view message) {
    wgen::tests::expectNear(actual.x, expected.x, 0.00001F, message);
    wgen::tests::expectNear(actual.y, expected.y, 0.00001F, message);
    wgen::tests::expectNear(actual.z, expected.z, 0.00001F, message);
}

void testCubeSphereLayoutAndValidation() {
    const wgen::CubeSphere<float> cubeSphere{42.0F, 5, 0.0F};
    wgen::tests::require(cubeSphere.resolution() == 5, "cube sphere resolution is wrong");
    wgen::tests::require(cubeSphere.width() == 5, "cube sphere buffer width is wrong");
    wgen::tests::require(cubeSphere.height() == 30, "cube sphere buffer height is wrong");
    wgen::tests::expectNear(cubeSphere.radius(), 42.0F, 0.00001F, "cube sphere radius is wrong");

    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::CubeSphere<float> invalid{wgen::HeightMap<float>{4, 4, 0.0F}}; },
        "cube sphere should reject a height map that is not N x 6N");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::CubeSphere<float> invalid{0.0F, 4, 0.0F}; },
        "cube sphere should reject a non-positive radius");
}

void testCubeSphereDirectionsAndSeams() {
    const wgen::CubeSphere<float> cubeSphere{5, 0.0F};
    const std::size_t center = cubeSphere.resolution() / 2;

    expectVectorNear(
        cubeSphere.pointUnitDir(wgen::CubeSphereFace::Top, center, center),
        {0.0F, 0.0F, 1.0F},
        "top face center direction is wrong");
    expectVectorNear(
        cubeSphere.pointUnitDir(wgen::CubeSphereFace::Bottom, center, center),
        {0.0F, 0.0F, -1.0F},
        "bottom face center direction is wrong");
    expectVectorNear(
        cubeSphere.pointUnitDir(wgen::CubeSphereFace::Right, center, center),
        {1.0F, 0.0F, 0.0F},
        "right face center direction is wrong");

    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < cubeSphere.resolution(); ++y) {
            for (std::size_t x = 0; x < cubeSphere.resolution(); ++x) {
                wgen::tests::expectNear(
                    glm::length(cubeSphere.pointUnitDir(face, x, y)),
                    1.0F,
                    0.00001F,
                    "cube sphere projection should produce unit directions");
            }
        }
    }

    for (std::size_t y = 0; y < cubeSphere.resolution(); ++y) {
        expectVectorNear(
            cubeSphere.pointUnitDir(wgen::CubeSphereFace::Top, cubeSphere.resolution() - 1, y),
            cubeSphere.pointUnitDir(wgen::CubeSphereFace::Right, 0, y),
            "adjacent cube sphere faces should share edge directions");
    }
}

void testEmptyPipelineReturnsZeroCubeSphere() {
    const wgen::TerrainPipeline3d pipeline;
    const auto cubeSphere = pipeline.generateCubeSphere(4);

    wgen::tests::require(cubeSphere.width() == 4, "empty 3D pipeline width is wrong");
    wgen::tests::require(cubeSphere.height() == 24, "empty 3D pipeline height is wrong");

    for (std::size_t y = 0; y < cubeSphere.height(); ++y) {
        for (std::size_t x = 0; x < cubeSphere.width(); ++x) {
            wgen::tests::expectNear(cubeSphere.at(x, y), 0.0F, 0.00001F, "empty 3D pipeline should produce zero cube sphere");
        }
    }
}

void testSingleGeneratorPipeline() {
    wgen::Generator3dPipelineSpec specs{
        wgen::Generator3dSpec{
            .kind = wgen::Generator3dKind::PerlinNoise,
            .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
        },
    };
    const auto pipeline = wgen::makePipeline3d(specs, 17);
    const wgen::PerlinNoise3d expectedGenerator{17, 0.5F};

    expectCubeSphereNear(
        pipeline->generateCubeSphere(5),
        expectedGenerator.generateCubeSphere(5),
        "single 3D generator pipeline result is wrong"
    );
}

void testGeneratorImpactFunction() {
    wgen::Generator3dPipelineSpec specs{
        wgen::Generator3dSpec{
            .kind = wgen::Generator3dKind::PerlinNoise,
            .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
            .scale = 2.5F,
        },
    };
    const auto pipeline = wgen::makePipeline3d(specs, 17);
    const wgen::PerlinNoise3d expectedGenerator{17, 0.5F};
    const auto expected = wgen::map(
        expectedGenerator.generateCubeSphere(5),
        wgen::multiplyFunction(2.5F)
    );

    expectCubeSphereNear(
        pipeline->generateCubeSphere(5),
        wgen::CubeSphere<float>{expected},
        "3D pipeline should apply generator impact function"
    );
}

void testNoiseMatchesCubeSphereSamples() {
    wgen::Generator3dPipelineSpec specs{
        wgen::Generator3dSpec{
            .kind = wgen::Generator3dKind::PerlinNoise,
            .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
            .scale = 0.5F,
        },
        wgen::Generator3dSpec{
            .kind = wgen::Generator3dKind::PerlinNoise,
            .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.25F},
            .scale = 2.0F,
        },
    };
    const auto pipeline = wgen::makePipeline3d(specs, 17);
    const auto cubeSphere = pipeline->generateCubeSphere(5);

    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < cubeSphere.resolution(); ++y) {
            for (std::size_t x = 0; x < cubeSphere.resolution(); ++x) {
                wgen::tests::expectNear(
                    pipeline->noise(cubeSphere.pointUnitDir(face, x, y)),
                    cubeSphere.at(face, x, y),
                    0.00001F,
                    "3D pipeline noise should match generated cube sphere sample"
                );
            }
        }
    }
}

void testSetSeedChainsGeneratorSeeds() {
    wgen::Generator3dPipelineSpec specs{
        wgen::Generator3dSpec{
            .kind = wgen::Generator3dKind::PerlinNoise,
            .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
        },
        wgen::Generator3dSpec{
            .kind = wgen::Generator3dKind::PerlinNoise,
            .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.25F},
        },
    };
    const auto pipeline = wgen::makePipeline3d(specs, 42);
    const wgen::PerlinNoise3d first{42, 0.5F};
    const wgen::PerlinNoise3d second{wgen::hashSeed(42), 0.25F};
    const auto expected = first.generateCubeSphere(5) + second.generateCubeSphere(5);

    expectCubeSphereNear(
        pipeline->generateCubeSphere(5),
        wgen::CubeSphere<float>{expected},
        "3D pipeline setSeed should seed each generator deterministically"
    );
}

void testOctaveFrequencyScalesCoordinates() {
    const wgen::Generator3dSpec spec{
        .kind = wgen::Generator3dKind::PerlinNoise,
        .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
        .octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = 2,
            .lacunarity = 2.0F,
            .persistance = 0.5F,
        },
    };
    const auto generator = wgen::makePipelineGenerator3d(spec, 17);
    const wgen::PerlinNoise3d expected{17, 0.5F};
    const glm::vec3 point{-0.25F, 0.75F, -0.5F};

    wgen::tests::expectNear(
        generator->noise(point),
        expected.noise(point * 4.0F),
        0.00001F,
        "3D octave frequency should scale coordinates"
    );
}

void testOctaveAmplitudeScalesPipelineOutput() {
    const wgen::Generator3dSpec spec{
        .kind = wgen::Generator3dKind::PerlinNoise,
        .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
        .scale = 1.5F,
        .octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = 2,
            .lacunarity = 2.0F,
            .persistance = 0.5F,
        },
    };
    const auto pipeline = wgen::makePipeline3d({spec}, 17);

    const wgen::PerlinNoise3d expectedGenerator{17, 0.5F};
    wgen::CubeSphere<float> expected{5, 0.0F};
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::size_t y = 0; y < expected.resolution(); ++y) {
            for (std::size_t x = 0; x < expected.resolution(); ++x) {
                expected.at(face, x, y) = expectedGenerator.noise(
                    expected.pointUnitDir(face, x, y) * 4.0F) * 1.5F * 0.25F;
            }
        }
    }

    expectCubeSphereNear(
        pipeline->generateCubeSphere(5),
        expected,
        "3D octave amplitude should scale pipeline output"
    );
}

void testInvalidOctaveSettingsThrow() {
    wgen::Generator3dSpec badLacunarity{
        .kind = wgen::Generator3dKind::PerlinNoise,
        .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
        .octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = 1,
            .lacunarity = 0.0F,
            .persistance = 0.5F,
        },
    };
    wgen::tests::requireThrows<std::invalid_argument>(
        [&] { wgen::makePipelineGenerator3d(badLacunarity, 17); },
        "3D generator should reject non-positive octave lacunarity"
    );

    wgen::Generator3dSpec badPersistence{
        .kind = wgen::Generator3dKind::PerlinNoise,
        .config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = 0.5F},
        .octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = 1,
            .lacunarity = 2.0F,
            .persistance = -0.1F,
        },
    };
    wgen::tests::requireThrows<std::invalid_argument>(
        [&] { wgen::makePipelineGenerator3d(badPersistence, 17); },
        "3D generator should reject negative octave persistence"
    );
}

void testPerlinNoiseIsDeterministic() {
    const wgen::PerlinNoise3d generator{17, 0.5F};
    const glm::vec3 point{-0.25F, 0.75F, -0.5F};

    wgen::tests::expectNear(
        generator.noise(point),
        generator.noise(point),
        0.00001F,
        "3D Perlin should be deterministic"
    );
}

void testPerlinNoiseDependsOnSeed() {
    const wgen::PerlinNoise3d first{17, 0.5F};
    const wgen::PerlinNoise3d second{29, 0.5F};
    const glm::vec3 point{-0.25F, 0.75F, -0.5F};

    wgen::tests::require(
        std::abs(first.noise(point) - second.noise(point)) > 0.00001F,
        "3D Perlin should depend on seed"
    );
}

void testPerlinRejectsInvalidCellSize() {
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::PerlinNoise3d generator{17, 0.0F}; },
        "3D Perlin should reject zero cell size"
    );
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("cube sphere layout and validation", testCubeSphereLayoutAndValidation);
        wgen::tests::runTest("cube sphere directions and seams", testCubeSphereDirectionsAndSeams);
        wgen::tests::runTest("empty 3D pipeline returns zero cube sphere", testEmptyPipelineReturnsZeroCubeSphere);
        wgen::tests::runTest("single 3D generator pipeline", testSingleGeneratorPipeline);
        wgen::tests::runTest("3D generator impact function", testGeneratorImpactFunction);
        wgen::tests::runTest("3D noise matches cube sphere samples", testNoiseMatchesCubeSphereSamples);
        wgen::tests::runTest("3D setSeed chains generator seeds", testSetSeedChainsGeneratorSeeds);
        wgen::tests::runTest("3D octave frequency scales coordinates", testOctaveFrequencyScalesCoordinates);
        wgen::tests::runTest("3D octave amplitude scales pipeline output", testOctaveAmplitudeScalesPipelineOutput);
        wgen::tests::runTest("3D invalid octave settings throw", testInvalidOctaveSettingsThrow);
        wgen::tests::runTest("3D Perlin is deterministic", testPerlinNoiseIsDeterministic);
        wgen::tests::runTest("3D Perlin depends on seed", testPerlinNoiseDependsOnSeed);
        wgen::tests::runTest("3D Perlin rejects invalid cell size", testPerlinRejectsInvalidCellSize);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
