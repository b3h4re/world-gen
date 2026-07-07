#include "terrain/generators/3d/generator_factory.hpp"
#include "terrain/generators/3d/noise/perlin.hpp"
#include "terrain/generators/3d/terrain_pipeline.hpp"
#include "terrain/utils/hash_random.hpp"

#include "helpers.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void expectPlanetNear(
        const wgen::Planet<float>& actual,
        const wgen::Planet<float>& expected,
        std::string_view message) {
    wgen::tests::expectMapNear(actual, expected, message);
}

void testEmptyPipelineReturnsZeroPlanet() {
    const wgen::TerrainPipeline3d pipeline;
    const auto planet = pipeline.generatePlanet(4);

    wgen::tests::require(planet.width() == 4, "empty 3D pipeline width is wrong");
    wgen::tests::require(planet.height() == 4, "empty 3D pipeline height is wrong");

    for (std::size_t y = 0; y < planet.height(); ++y) {
        for (std::size_t x = 0; x < planet.width(); ++x) {
            wgen::tests::expectNear(planet.at(x, y), 0.0F, 0.00001F, "empty 3D pipeline should produce zero planet");
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

    expectPlanetNear(
        pipeline->generatePlanet(5),
        expectedGenerator.generatePlanet(5),
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
        expectedGenerator.generatePlanet(5),
        wgen::multiplyFunction(2.5F)
    );

    expectPlanetNear(
        pipeline->generatePlanet(5),
        wgen::Planet<float>{expected},
        "3D pipeline should apply generator impact function"
    );
}

void testNoiseMatchesPlanetSamples() {
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
    const auto planet = pipeline->generatePlanet(5);

    for (std::size_t y = 0; y < planet.height(); ++y) {
        for (std::size_t x = 0; x < planet.width(); ++x) {
            wgen::tests::expectNear(
                pipeline->noise(planet.pointUnitDir(x, y)),
                planet.at(x, y),
                0.00001F,
                "3D pipeline noise should match generated planet sample"
            );
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
    const auto expected = first.generatePlanet(5) + second.generatePlanet(5);

    expectPlanetNear(
        pipeline->generatePlanet(5),
        wgen::Planet<float>{expected},
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
    wgen::Planet<float> expected{5, 0.0F};
    for (std::size_t y = 0; y < expected.height(); ++y) {
        for (std::size_t x = 0; x < expected.width(); ++x) {
            expected.at(x, y) = expectedGenerator.noise(expected.pointUnitDir(x, y) * 4.0F) * 1.5F * 0.25F;
        }
    }

    expectPlanetNear(
        pipeline->generatePlanet(5),
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
        wgen::tests::runTest("empty 3D pipeline returns zero planet", testEmptyPipelineReturnsZeroPlanet);
        wgen::tests::runTest("single 3D generator pipeline", testSingleGeneratorPipeline);
        wgen::tests::runTest("3D generator impact function", testGeneratorImpactFunction);
        wgen::tests::runTest("3D noise matches planet samples", testNoiseMatchesPlanetSamples);
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
