#include "device/lve_compute_device.hpp"
#include "renderer/compute/computer.hpp"
#include "renderer/compute/gpu_height_map.hpp"
#include "terrain/generators/noise/value_noise.hpp"

#include "helpers.hpp"

#include <cstdint>
#include <exception>
#include <iostream>

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

template<class GenType>
requires wgen::valid_generator<GenType>
void testgenerator(GenType& gen, std::string& message) {
    const std::size_t width = 1000;
    const std::size_t height = 1000;
    wgen::SeedType seeds[10] = {0, 42, 12323, 12333214, 1287612763};
    lve::Computer computer{*device, "value_noise", ValueNoiseComputeSpec{}};

    for (const auto& seed : seeds) {
        gen.setSeed(seed);

        lve::GpuHeightMap gpuHeightMap{*device, width, height};
        const ValueNoiseComputeSpec spec{
            .width = static_cast<std::uint32_t>(width),
            .height = static_cast<std::uint32_t>(height),
            .seed = seed,
        };

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
    wgen::ValueNoiseGenerator gen{};
    std::string message = "Value Noise generated on cpu must be exactly the same as generated on GPU";
    testgenerator<wgen::ValueNoiseGenerator>(gen, message);
}

void testPerlinNoise() {
    wgen::PerlinNoise2d gen{100, 100, 100};
    std::string message = "Perlin Noise generated on cpu must be exactly the same as generated on GPU";
    testgenerator<wgen::PerlinNoise2d>(gen, message);
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
