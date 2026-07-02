#include "gpu_height_map_accumulator.hpp"

#include <array>
#include <limits>
#include <span>
#include <stdexcept>

namespace lve {

std::uint32_t checkedSampleCount(const GpuHeightMap& heightMap) {
    if (heightMap.width() > std::numeric_limits<std::size_t>::max() / heightMap.height()) {
        throw std::overflow_error("GPU heightmap sample count overflows size_t");
    }

    const std::size_t sampleCount = heightMap.width() * heightMap.height();
    if (sampleCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("GPU heightmap sample count overflows uint32_t");
    }

    return static_cast<std::uint32_t>(sampleCount);
}

GpuHeightMapAccumulator::GpuHeightMapAccumulator(LveComputeDevice& device)
    : computer_{device, "accumulate_height", sizeof(AccumulateHeightMapSpec), 2} {}

void GpuHeightMapAccumulator::accumulate(const GpuHeightMap& input, GpuHeightMap& output, float scale) const {
    if (input.width() != output.width() || input.height() != output.height()) {
        throw std::invalid_argument("GPU heightmap accumulation requires matching dimensions");
    }

    const AccumulateHeightMapSpec spec{
        .sampleCount = checkedSampleCount(input),
        .scale = scale,
    };
    const std::array<VkDescriptorBufferInfo, 2> buffers{
        input.readOnlyDescriptorInfo(),
        output.descriptorInfo(),
    };

    computer_.dispatch(
        spec,
        std::span<const VkDescriptorBufferInfo>{buffers.data(), buffers.size()},
        ComputeDispatchSize{
            .groupCountX = Computer::dispatchGroupCount(spec.sampleCount, 256),
            .groupCountY = 1,
            .groupCountZ = 1,
        });
}

} // namespace lve
