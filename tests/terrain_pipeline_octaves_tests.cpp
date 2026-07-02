#include "terrain/generators/generator_factory.hpp"
#include "terrain/generators/noise/noise_octaves.hpp"
#include "terrain/generators/noise/perlin.hpp"
#include "terrain/generators/noise/simplex.hpp"
#include "terrain/generators/noise/value_noise.hpp"
#include "terrain/generators/noise/worley.hpp"

#include "helpers.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t WIDTH = 12;
constexpr std::size_t HEIGHT = 10;
constexpr wgen::SeedType SEED = 12345;
constexpr float SCALE = 1.25F;

const wgen::GeneratorOctaveSettings OCTAVES{
    .numOctaves = 4,
    .lacunarity = 2.0F,
    .persistance = 0.5F,
};

void expectPipelineMatchesExpected(wgen::GeneratorSpec spec, const wgen::Generator& expected, std::string_view message) {
    auto pipeline = wgen::makePipeline({spec}, SEED);
    const wgen::HeightMap<float> actualHeightMap = pipeline->generateHeightMap(WIDTH, HEIGHT);
    const wgen::HeightMap<float> expectedHeightMap =
        wgen::map(expected.generateHeightMap(WIDTH, HEIGHT), wgen::multiplyFunction(spec.scale));

    if (actualHeightMap.isClose(expectedHeightMap, 0.00001F)) {
        return;
    }

    float maxDiff = 0.0F;
    std::size_t maxX = 0;
    std::size_t maxY = 0;
    for (std::size_t y = 0; y < HEIGHT; ++y) {
        for (std::size_t x = 0; x < WIDTH; ++x) {
            const float diff = std::abs(actualHeightMap.at(x, y) - expectedHeightMap.at(x, y));
            if (diff > maxDiff) {
                maxDiff = diff;
                maxX = x;
                maxY = y;
            }
        }
    }

    std::ostringstream stream;
    stream << message << " max diff " << maxDiff << " at (" << maxX << ", " << maxY << ")"
           << " actual " << actualHeightMap.at(maxX, maxY)
           << " expected " << expectedHeightMap.at(maxX, maxY);
    wgen::tests::require(false, stream.str());
}

void testValueNoiseOctavesMatchOctaveGenerator() {
    const wgen::GeneratorSpec spec{
        .kind = wgen::GeneratorKind::ValueNoise,
        .config = wgen::ValueNoiseGeneratorSpec{},
        .scale = SCALE,
        .computeMethod = wgen::TerrainComputeMethod::Cpu,
        .octaveSettings = OCTAVES,
    };
    const wgen::OctaveGenerator<wgen::ValueNoiseGenerator> expected{
        SEED,
        OCTAVES.numOctaves,
        OCTAVES.lacunarity,
        OCTAVES.persistance,
    };

    expectPipelineMatchesExpected(spec, expected, "value noise pipeline octaves should match OctaveGenerator");
}

void testPerlinOctavesMatchOctaveGenerator() {
    constexpr std::size_t dots = 5;
    const wgen::GeneratorSpec spec{
        .kind = wgen::GeneratorKind::PerlinNoise,
        .config = wgen::PerlinNoiseGeneratorSpec{
            .dotsPerCell = dots,
        },
        .scale = SCALE,
        .computeMethod = wgen::TerrainComputeMethod::Cpu,
        .octaveSettings = OCTAVES,
    };
    const wgen::OctaveGenerator<wgen::PerlinNoise2d> expected{
        dots,
        SEED,
        OCTAVES.numOctaves,
        OCTAVES.lacunarity,
        OCTAVES.persistance,
    };

    expectPipelineMatchesExpected(spec, expected, "perlin pipeline octaves should match OctaveGenerator");
}

void testWorleyOctavesMatchOctaveGenerator() {
    constexpr std::size_t dots = 5;
    const wgen::GeneratorSpec spec{
        .kind = wgen::GeneratorKind::WorleyNoise,
        .config = wgen::WorleyNoiseGeneratorSpec{
            .dotsPerCell = dots,
        },
        .scale = SCALE,
        .computeMethod = wgen::TerrainComputeMethod::Cpu,
        .octaveSettings = OCTAVES,
    };
    const wgen::OctaveGenerator<wgen::WorleyNoise2d> expected{
        dots,
        SEED,
        OCTAVES.numOctaves,
        OCTAVES.lacunarity,
        OCTAVES.persistance,
    };

    expectPipelineMatchesExpected(spec, expected, "worley pipeline octaves should match OctaveGenerator");
}

void testSimplexOctavesMatchOctaveGenerator() {
    constexpr std::size_t dots = 5;
    const wgen::GeneratorSpec spec{
        .kind = wgen::GeneratorKind::SimplexNoise,
        .config = wgen::SimplexNoiseGeneratorSpec{
            .dotsPerCell = dots,
        },
        .scale = SCALE,
        .computeMethod = wgen::TerrainComputeMethod::Cpu,
        .octaveSettings = OCTAVES,
    };
    const wgen::OctaveGenerator<wgen::SimplexNoise2d> expected{
        dots,
        SEED,
        OCTAVES.numOctaves,
        OCTAVES.lacunarity,
        OCTAVES.persistance,
    };

    expectPipelineMatchesExpected(spec, expected, "simplex pipeline octaves should match OctaveGenerator");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("value noise octaves match OctaveGenerator", testValueNoiseOctavesMatchOctaveGenerator);
        wgen::tests::runTest("perlin octaves match OctaveGenerator", testPerlinOctavesMatchOctaveGenerator);
        wgen::tests::runTest("worley octaves match OctaveGenerator", testWorleyOctavesMatchOctaveGenerator);
        wgen::tests::runTest("simplex octaves match OctaveGenerator", testSimplexOctavesMatchOctaveGenerator);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
