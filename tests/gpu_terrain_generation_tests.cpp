#include "device/lve_compute_device.hpp"
#include "renderer/compute/computer.hpp"
#include "renderer/compute/gpu_height_map.hpp"
#include "renderer/compute/gpu_terrain_pipeline.hpp"
#include "terrain/generators/generator_spec.hpp"
#include "terrain/generators/noise/perlin.hpp"
#include "terrain/generators/noise/value_noise.hpp"

#include "helpers.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace {

constexpr float EPS = 0.000001;
constexpr std::uint32_t LOCAL_SIZE_X = 16;
constexpr std::uint32_t LOCAL_SIZE_Y = 16;

struct ValueNoiseComputeSpec {
    std::uint32_t width{};
    std::uint32_t height{};
    wgen::SeedType seed{};
};

lve::LveComputeDevice* device;

bool isVulkanUnavailableError(const std::exception& exception) {
    const std::string message = exception.what();
    return message.find("failed to create compute Vulkan instance") != std::string::npos ||
           message.find("failed to find a Vulkan device with compute queue support") != std::string::npos ||
           message.find("Found no drivers") != std::string::npos;
}


template<class GenType, class GenSpec>
requires wgen::valid_generator<GenType>
void testGenerator(
        std::size_t width,
        std::size_t height,
        GenType& gen,
        GenSpec spec,
        const std::string& message,
        float epsilon = EPS,
        std::uint32_t localSizeX = LOCAL_SIZE_X,
        std::uint32_t localSizeY = LOCAL_SIZE_Y) {
    wgen::SeedType seeds[10] = {0, 42, 12323, 12333214, 1287612763};
    lve::Computer computer{*device, gen.compShader(), gen.specSize()};

    for (const auto& seed : seeds) {
        gen.setSeed(seed);
        spec.seed = seed;

        lve::GpuHeightMap gpuHeightMap{*device, width, height};


        computer.dispatch(
            spec,
            gpuHeightMap.descriptorInfo(),
            lve::ComputeDispatchSize{
                .groupCountX = lve::Computer::dispatchGroupCount(width, localSizeX),
                .groupCountY = lve::Computer::dispatchGroupCount(height, localSizeY),
                .groupCountZ = 1,
            }
        );

        wgen::HeightMap<float> heightMapGpu = gpuHeightMap.copyToCpu();
        wgen::HeightMap<float> heightMapCpu = gen.generateHeightMap(width, height);

        wgen::tests::require(heightMapGpu.isClose(heightMapCpu, epsilon), message);
    }

}


void testValueNoise() {
    const std::size_t width = 1000;
    const std::size_t height = 1000;

    wgen::ValueNoiseGenerator gen{};
    wgen::ValueNoiseComputeSpec spec{
        .width = width,
        .height = height,
    };
    std::string message = "Value Noise generated on cpu must be exactly the same as generated on GPU";
    testGenerator<wgen::ValueNoiseGenerator>(width, height, gen, spec, message);
}

void testPerlinNoise() {
    const std::size_t width = 1000;
    const std::size_t height = 1000;
    const std::size_t dots = 1000;

    wgen::PerlinNoise2d gen{dots, 0};
    wgen::PerlinNoiseComputeSpec spec{
        .dots = dots
    };
    std::string message = "Perlin Noise generated on cpu must be exactly the same as generated on GPU";
    testGenerator<wgen::PerlinNoise2d>(width, height, gen, spec, message, 0.00001, 1, 1);
}

void testGpuTerrainPipeline() {
    const std::size_t width = 64;
    const std::size_t height = 64;
    const wgen::SeedType valueSeed = 42;
    const wgen::SeedType perlinSeed = 711;
    const float valueScale = 0.35F;
    const float perlinScale = 1.75F;

    const wgen::GeneratorSpec valueSpec{
        .kind = wgen::GeneratorKind::ValueNoise,
        .config = wgen::ValueNoiseGeneratorSpec{},
        .scale = valueScale,
        .computeMethod = wgen::TerrainComputeMethod::VulkanCompute,
    };
    const wgen::GeneratorSpec perlinSpec{
        .kind = wgen::GeneratorKind::PerlinNoise,
        .config = wgen::PerlinNoiseGeneratorSpec{
            .dotsPerCell = 16,
        },
        .scale = perlinScale,
        .computeMethod = wgen::TerrainComputeMethod::VulkanCompute,
    };

    lve::GpuTerrainPipeline pipeline;
    const wgen::HeightMap<float> gpuHeightMap = pipeline.generateHeightMap(
        {
            lve::GpuGeneratorRequest{
                .spec = valueSpec,
                .seed = valueSeed,
            },
            lve::GpuGeneratorRequest{
                .spec = perlinSpec,
                .seed = perlinSeed,
            },
        },
        width,
        height);

    wgen::ValueNoiseGenerator valueNoise{valueSeed};
    wgen::PerlinNoise2d perlinNoise{16, perlinSeed};
    const wgen::HeightMap<float> cpuHeightMap =
        wgen::map(valueNoise.generateHeightMap(width, height), wgen::multiplyFunction(valueScale)) +
        wgen::map(perlinNoise.generateHeightMap(width, height), wgen::multiplyFunction(perlinScale));

    wgen::tests::require(
        gpuHeightMap.isClose(cpuHeightMap, 0.00001F),
        "GPU terrain pipeline must accumulate scaled generator outputs");
}


}

int main() {
    try {
        lve::LveComputeDevice computeDevice{};
        device = &computeDevice;

        wgen::tests::runTest("Value Noise Test", testValueNoise);
        wgen::tests::runTest("Perlin Noise Test", testPerlinNoise);
        wgen::tests::runTest("GPU Terrain Pipeline Test", testGpuTerrainPipeline);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << "\n";
        if (isVulkanUnavailableError(exception)) {
            return 77;
        }
        return 1;
    }



    return 0;
}
