#include "terrain/generators/noise/value_noise.hpp"
#include "terrain/generators/terrain_pipeline.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

void expectNear(float actual, float expected, float epsilon, std::string_view message) {
    if (std::abs(actual - expected) > epsilon) {
        throw std::runtime_error{std::string{message}};
    }
}

void expectMapNear(
    const wgen::HeightMap<float>& actual,
    const wgen::HeightMap<float>& expected,
    std::string_view message
) {
    require(actual.width() == expected.width(), "heightmap width mismatch");
    require(actual.height() == expected.height(), "heightmap height mismatch");

    for (std::size_t y = 0; y < actual.height(); ++y) {
        for (std::size_t x = 0; x < actual.width(); ++x) {
            expectNear(actual.at(x, y), expected.at(x, y), 0.00001F, message);
        }
    }
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
    const wgen::TerrainPipeline pipeline{
        "value_noise seed=17",
    };

    const wgen::ValueNoiseGenerator expectedGenerator{17};
    expectMapNear(
        pipeline.generateHeightMap(4, 3),
        expectedGenerator.generateHeightMap(4, 3),
        "single generator pipeline result is wrong"
    );
}

void testMultipleGeneratorPipeline() {
    const wgen::TerrainPipeline pipeline{
        "value_noise seed=17",
        "value_noise seed=29",
    };

    const wgen::ValueNoiseGenerator first{17};
    const wgen::ValueNoiseGenerator second{29};
    const auto expected = first.generateHeightMap(4, 3) + second.generateHeightMap(4, 3);

    expectMapNear(
        pipeline.generateHeightMap(4, 3),
        expected,
        "multiple generator pipeline result is wrong"
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
        runTest("empty pipeline returns zero heightmap", testEmptyPipelineReturnsZeroHeightMap);
        runTest("single generator pipeline", testSingleGeneratorPipeline);
        runTest("multiple generator pipeline", testMultipleGeneratorPipeline);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
