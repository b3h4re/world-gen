#include "device/lve_compute_device.hpp"
#include "terrain/generators/noise/value_noise.hpp"
#include "renderer/compute/value_noise_compute.hpp"

#include "helpers.hpp"

#include <exception>
#include <iostream>

namespace {

lve::LveComputeDevice device{};

constexpr float EPS = 0.000001;

void testValueNoise() {
    const std::size_t width = 1000;
    const std::size_t height = 1000;
    wgen::SeedType seeds[10] = {0, 42, 12323, 12333214, 1287612763};

    for (const auto& seed : seeds) {
        wgen::ValueNoiseGenerator gen{seed};

        lve::VulkanValueNoiseGeneratorDevice valueNoiseGpu{device};

        wgen::HeightMap<float> heightMapGpu = valueNoiseGpu.generateHeightMap(gen, width, height);
        wgen::HeightMap<float> heightMapCpu = gen.generateHeightMap(width, height);

        wgen::tests::require(heightMapGpu.isClose(heightMapCpu, EPS),
            "Value Noise generated on cpu must be exactly the same as generated on GPU");
    }
}



}

int main() {
    try {
        wgen::tests::runTest("Value Noise Test", testValueNoise);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << "\n";
        return 77;
    }



    return 0;
}
