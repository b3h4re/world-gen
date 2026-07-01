#include "device/lve_compute_device.hpp"
#include "renderer/compute/computer.hpp"
#include "renderer/compute/gpu_height_map.hpp"
#include "terrain/generators/noise/value_noise.hpp"

#include "helpers.hpp"

#include <cstdint>
#include <exception>
#include <iostream>

namespace {

constexpr float EPS = 0.00001;
constexpr std::uint32_t LOCAL_SIZE_X = 16;
constexpr std::uint32_t LOCAL_SIZE_Y = 16;

struct ValueNoiseComputeSpec {
    std::uint32_t width{};
    std::uint32_t height{};
    wgen::SeedType seed{};
};

lve::LveComputeDevice* device;


template<class GenType, class GenSpec>
requires wgen::valid_generator<GenType>
void testGenerator(std::size_t width, std::size_t height, GenType& gen, GenSpec spec, const std::string& message) {
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
                .groupCountX = lve::Computer::dispatchGroupCount(width, LOCAL_SIZE_X),
                .groupCountY = lve::Computer::dispatchGroupCount(height, LOCAL_SIZE_Y),
                .groupCountZ = 1,
            }
        );

        wgen::HeightMap<float> heightMapGpu = gpuHeightMap.copyToCpu();
        wgen::HeightMap<float> heightMapCpu = gen.generateHeightMap(width, height);

        wgen::tests::require(heightMapGpu.isClose(heightMapCpu, EPS), message);
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

    wgen::PerlinNoise2d gen{width, height, dots};
    wgen::PerlinNoiseComputeSpec spec{
        .width = width,
        .height = height,
        .dots = dots
    };
    std::string message = "Perlin Noise generated on cpu must be exactly the same as generated on GPU";
    testGenerator<wgen::PerlinNoise2d>(width, height, gen, spec, message);
}


}

int main() {
    try {
        lve::LveComputeDevice computeDevice{};
        device = &computeDevice;

        wgen::tests::runTest("Value Noise Test", testValueNoise);
        wgen::tests::runTest("Perlin Noise Test", testPerlinNoise);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << "\n";
        return 1;
    }



    return 0;
}
