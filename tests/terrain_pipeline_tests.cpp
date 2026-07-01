#include "terrain/generators/noise/value_noise.hpp"
#include "terrain/generators/terrain_pipeline.hpp"
#include "terrain/utils/hash_random.hpp"

#include "helpers.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {

void testMakeGeneratorForwardsConstructorArguments() {
    const auto generator = wgen::makeGenerator<wgen::ValueNoiseGenerator>(17);

    require(generator != nullptr, "makeGenerator should return a generator");
    require(generator->getSeed() == 17, "makeGenerator should forward constructor arguments");
}

void testEmptyPipelineReturnsZeroHeightMap() {
    const wgen::TerrainPipeline pipeline;
    const auto map = pipeline.generateHeightMap(3, 2);

    require(map.width() == 3, "empty pipeline width is wrong");
    require(map.height() == 2, "empty pipeline height is wrong");

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            expectNear(map.at(x, y), 0.0F, 0.00001F, "empty pipeline should produce zero heightmap");
        }
    }
}

void testSingleGeneratorPipeline() {
    wgen::TerrainPipeline pipeline;
    pipeline.push_back<wgen::ValueNoiseGenerator>(17);

    const wgen::ValueNoiseGenerator expectedGenerator{17};
    expectMapNear(
        pipeline.generateHeightMap(4, 3),
        expectedGenerator.generateHeightMap(4, 3),
        "single generator pipeline result is wrong"
    );
}

void testMultipleGeneratorPipeline() {
    wgen::TerrainPipeline pipeline;
    pipeline.push_back<wgen::ValueNoiseGenerator>(17);
    pipeline.push_back<wgen::ValueNoiseGenerator>(29);

    const wgen::ValueNoiseGenerator first{17};
    const wgen::ValueNoiseGenerator second{29};
    const auto expected = first.generateHeightMap(4, 3) + second.generateHeightMap(4, 3);

    expectMapNear(
        pipeline.generateHeightMap(4, 3),
        expected,
        "multiple generator pipeline result is wrong"
    );
}

void testGeneratorImpactFunction() {
    wgen::TerrainPipeline pipeline;
    pipeline.push_back<wgen::ValueNoiseGenerator>(wgen::multiplyFunction(2.5F), 17);

    const wgen::ValueNoiseGenerator generator{17};
    const auto expected = wgen::map(
        generator.generateHeightMap(4, 3),
        wgen::multiplyFunction(2.5F)
    );

    expectMapNear(
        pipeline.generateHeightMap(4, 3),
        expected,
        "pipeline should apply generator impact function"
    );
}

void testNoiseMatchesHeightMapSamples() {
    wgen::TerrainPipeline pipeline;
    pipeline.push_back<wgen::ValueNoiseGenerator>(wgen::multiplyFunction(0.5F), 17);
    pipeline.push_back<wgen::ValueNoiseGenerator>(wgen::multiplyFunction(2.0F), 29);

    const auto map = pipeline.generateHeightMap(4, 3);

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            expectNear(
                pipeline.noise(x, y),
                map.at(x, y),
                0.00001F,
                "pipeline noise should match generated heightmap sample"
            );
        }
    }
}

void testSetSeedChainsGeneratorSeeds() {
    wgen::TerrainPipeline pipeline;
    pipeline.push_back<wgen::ValueNoiseGenerator>();
    pipeline.push_back<wgen::ValueNoiseGenerator>();

    pipeline.setSeed(42);

    const wgen::ValueNoiseGenerator first{42};
    const wgen::ValueNoiseGenerator second{wgen::hashSeed(42)};
    const auto expected = first.generateHeightMap(4, 3) + second.generateHeightMap(4, 3);

    expectMapNear(
        pipeline.generateHeightMap(4, 3),
        expected,
        "pipeline setSeed should seed each generator deterministically"
    );
}

using TestFunction = void (*)();

void runTest(std::string_view name, TestFunction test) {
    try {
        test();
    } catch (const std::exception& exception) {
        throw std::runtime_error{std::string{name} + ": " + exception.what()};
    }
}

} // namespace

int main() {
    try {
        runTest("makeGenerator forwards constructor arguments", testMakeGeneratorForwardsConstructorArguments);
        runTest("empty pipeline returns zero heightmap", testEmptyPipelineReturnsZeroHeightMap);
        runTest("single generator pipeline", testSingleGeneratorPipeline);
        runTest("multiple generator pipeline", testMultipleGeneratorPipeline);
        runTest("generator impact function", testGeneratorImpactFunction);
        runTest("noise matches heightmap samples", testNoiseMatchesHeightMapSamples);
        runTest("setSeed chains generator seeds", testSetSeedChainsGeneratorSeeds);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
