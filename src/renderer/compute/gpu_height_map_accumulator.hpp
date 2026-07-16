#pragma once

#include "device/lve_compute_device.hpp"
#include "renderer/compute/computer.hpp"
#include "renderer/compute/gpu_height_map.hpp"

#include <cstdint>

namespace lve {

struct AccumulateHeightMapSpec {
    std::uint32_t sampleCount{};
    float scale{1.0F};
    float bias{};
};

class GpuHeightMapAccumulator {
public:
    explicit GpuHeightMapAccumulator(LveComputeDevice& device);

    void accumulate(
        const GpuHeightMap& input,
        GpuHeightMap& output,
        float scale,
        float bias = 0.0F) const;

private:
    Computer computer_;
};

} // namespace lve
