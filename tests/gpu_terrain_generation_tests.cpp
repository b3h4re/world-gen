#include "device/lve_compute_device.hpp"
#include "terrain/generators/noise/value_noise.hpp"
#include "renderer/compute/value_noise_compute.hpp"

#include <exception>
#include <iostream>

namespace {

constexpr float EPS = 0.000001;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool equals(const wgen::HeightMap<float>& h1, const wgen::HeightMap<float>& h2) {
    if (h1.width() != h2.width() || h1.height() != h2.height()) {
        return false;
    }

    for (std::size_t x = 0; x < h1.width(); ++x) {
        for (std::size_t y = 0; y < h1.height(); ++y) {
            if (std::abs(h1.at(x, y) - h2.at(x, y)) > EPS) {
                return false;
            }
        }
    }
    return true;
}

void testValueNoise(lve::LveComputeDevice& device) {
    const std::size_t width = 1000;
    const std::size_t height = 1000;
    std::uint32_t seeds[10] = {0, 42, 12323, 12333214, 1287612763};

    for (const auto& seed : seeds) {
        wgen::ValueNoiseGenerator gen{seed};

        lve::VulkanValueNoiseGeneratorDevice valueNoiseGpu{device};

        wgen::HeightMap<float> heightMapGpu = valueNoiseGpu.generateHeightMap(gen, width, height);
        wgen::HeightMap<float> heightMapCpu = gen.generateHeightMap(width, height);

        require(equals(heightMapGpu, heightMapCpu),
            "Value Noise generated on cpu must be exactly the same as generated on GPU");
    }
}



}

int main() {
    try {
        lve::LveComputeDevice device{};

        if (device.device() == VK_NULL_HANDLE || device.computeQueue() == VK_NULL_HANDLE) {
            std::cerr << "failed to create compute device handles\n";
            return 1;
        }

        testValueNoise(device);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << "\n";
        return 77;
    }



    return 0;
}
