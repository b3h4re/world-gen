#include "device/lve_compute_device.hpp"
#include "renderer/compute/computer.hpp"
#include "renderer/compute/gpu_height_map.hpp"
#include "renderer/compute/gpu_terrain_pipeline.hpp"
#include "terrain/generators/generator_factory.hpp"
#include "terrain/generators/generator_spec.hpp"
#include "terrain/generators/noise/perlin.hpp"
#include "terrain/generators/noise/simplex.hpp"
#include "terrain/generators/noise/value_noise.hpp"
#include "terrain/generators/noise/worley.hpp"

#include "helpers.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
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

        std::string message_specific = message + " seed = " + std::to_string(seed);

        wgen::tests::require(heightMapGpu.isClose(heightMapCpu, epsilon), message_specific);
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

void testWorleyNoise() {
    const std::size_t width = 100;
    const std::size_t height = 100;
    const std::size_t dots = 1000;
    const std::size_t numPoints = 1;
    const float p = 2.0F;

    wgen::WorleyNoise2d gen{dots, 0, p, numPoints};
    wgen::WorleyNoiseComputeSpec spec{
        .dots = dots,
        .p = p
    };
    std::string message = "Worley Noise generated on cpu must be exactly the same as generated on GPU";
    testGenerator<wgen::WorleyNoise2d>(width, height, gen, spec, message, 0.00001, 1, 1);
}

void testWorleyNoiseTwoPoints() {
    const std::size_t width = 100;
    const std::size_t height = 100;
    const std::size_t dots = 1000;
    const std::size_t numPoints = 2;
    const float p = 2.0F;

    wgen::WorleyNoise2d gen{dots, 0, p, numPoints};
    wgen::WorleyNoiseComputeSpec spec{
        .dots = dots,
        .p = p
    };
    std::string message = "Worley Noise with two feature points generated on cpu must be exactly the same as generated on GPU";
    testGenerator<wgen::WorleyNoise2d>(width, height, gen, spec, message, 0.00001, 1, 1);
}

void testSimplexNoise() {
    const std::size_t width = 1000;
    const std::size_t height = 1000;
    const std::size_t dots = 1000;

    wgen::SimplexNoise2d gen{dots, 0};
    wgen::SimplexNoiseComputeSpec spec{
        .dots = dots,
    };
    std::string message = "Worley Noise generated on cpu must be exactly the same as generated on GPU";
    testGenerator<wgen::SimplexNoise2d>(width, height, gen, spec, message, 0.00001, 1, 1);
}

void testWaveletNoise() {
    const float A = wgen::WaveletNoise2d::A;
    const float B = wgen::WaveletNoise2d::B;
    const float C = wgen::WaveletNoise2d::C;
    const float frequency = wgen::WaveletNoise2d::DEFAULT_FREQUENCY;
    const glm::vec4 p{A, B, C, frequency};
    const std::size_t kWidth = 5;
    const std::size_t kheight = 5;
    const std::size_t width = 100;
    const std::size_t height = 100;

    wgen::WaveletNoise2d gen{
        {kWidth, kheight}, p
    };
    wgen::WaveletNoiseComputeSpec spec{
        .waveletParams = p,
        .kWidth = kWidth,
        .kHeight = kheight
    };
    std::string message = "Wavelet Noise generated on cpu must be exactly the same as generated on GPU";
    testGenerator<wgen::WaveletNoise2d>(width, height, gen, spec, message, 0.00001, 1, 1);
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

void testGpuTerrainPipelineWorleyTwoPoints() {
    const std::size_t width = 64;
    const std::size_t height = 64;
    const wgen::SeedType worleySeed = 123;
    const float worleyScale = 1.0F;
    const std::size_t dots = 16;
    const std::size_t numPoints = 2;
    const float p = 2.0F;

    const wgen::GeneratorSpec worleySpec{
        .kind = wgen::GeneratorKind::WorleyNoise,
        .config = wgen::WorleyNoiseGeneratorSpec{
            .dotsPerCell = dots,
            .numPoints = numPoints,
            .p = p,
        },
        .scale = worleyScale,
        .computeMethod = wgen::TerrainComputeMethod::VulkanCompute,
    };

    lve::GpuTerrainPipeline pipeline;
    const wgen::HeightMap<float> gpuHeightMap = pipeline.generateHeightMap(
        {
            lve::GpuGeneratorRequest{
                .spec = worleySpec,
                .seed = worleySeed,
            },
        },
        width,
        height);

    lve::GpuHeightMap directGpuHeightMap{*device, width, height};
    const wgen::WorleyNoiseComputeSpec computeSpec{
        .dots = dots,
        .p = p,
        .seed = worleySeed,
    };
    lve::Computer computer{*device, "worley_noise_2", sizeof(wgen::WorleyNoiseComputeSpec)};
    computer.dispatch(
        computeSpec,
        directGpuHeightMap.descriptorInfo(),
        lve::ComputeDispatchSize{
            .groupCountX = width,
            .groupCountY = height,
            .groupCountZ = 1,
        });
    const wgen::HeightMap<float> directGpuWorleyHeightMap = directGpuHeightMap.copyToCpu();

    wgen::tests::require(
        gpuHeightMap.isClose(directGpuWorleyHeightMap, 0.00001F),
        "GPU terrain pipeline must choose the two-point Worley shader");
}

void testGpuTerrainPipelinePerlinOctave() {
    const std::size_t width = 64;
    const std::size_t height = 64;
    const wgen::SeedType seed = 1234;

    const wgen::GeneratorSpec perlinSpec{
        .kind = wgen::GeneratorKind::PerlinNoise,
        .config = wgen::PerlinNoiseGeneratorSpec{
            .dotsPerCell = 16,
        },
        .scale = 1.25F,
        .computeMethod = wgen::TerrainComputeMethod::VulkanCompute,
        .octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = 2,
            .lacunarity = 2.0F,
            .persistance = 0.5F,
        },
    };

    lve::GpuTerrainPipeline gpuPipeline;
    const wgen::HeightMap<float> gpuHeightMap = gpuPipeline.generateHeightMap(
        {
            lve::GpuGeneratorRequest{
                .spec = perlinSpec,
                .seed = seed,
            },
        },
        width,
        height);

    const std::unique_ptr<wgen::TerrainPipeline> cpuPipeline = wgen::makePipeline({perlinSpec}, seed);
    const wgen::HeightMap<float> cpuHeightMap = cpuPipeline->generateHeightMap(width, height);

    wgen::tests::require(
        gpuHeightMap.isClose(cpuHeightMap, 0.00001F),
        "GPU terrain pipeline must apply octave coordinate scale and amplitude on GPU");
}


}

int main() {
    try {
        lve::LveComputeDevice computeDevice{};
        device = &computeDevice;

        wgen::tests::runTest("Wavelet Noise Test", testWaveletNoise);
        wgen::tests::runTest("Value Noise Test", testValueNoise);
        wgen::tests::runTest("Perlin Noise Test", testPerlinNoise);
        wgen::tests::runTest("Worley Noise Test", testWorleyNoise);
        wgen::tests::runTest("Worley Noise Two Points Test", testWorleyNoiseTwoPoints);
        wgen::tests::runTest("Simplex Noise Test", testSimplexNoise);
        wgen::tests::runTest("GPU Terrain Pipeline Test", testGpuTerrainPipeline);
        wgen::tests::runTest("GPU Terrain Pipeline Worley Two Points Test", testGpuTerrainPipelineWorleyTwoPoints);
        wgen::tests::runTest("GPU Terrain Pipeline Perlin Octave Test", testGpuTerrainPipelinePerlinOctave);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << "\n";
        if (isVulkanUnavailableError(exception)) {
            return 77;
        }
        return 1;
    }



    return 0;
}
