#pragma once

#include "device/lve_compute_device.hpp"
#include "device/lve_device.hpp"
#include "model/buffer/lve_buffer.hpp"
#include "terrain/terrain.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace lve {

class GpuHeightMap {
public:
    GpuHeightMap(LveDevice& device, std::size_t width, std::size_t height);
    GpuHeightMap(LveComputeDevice& device, std::size_t width, std::size_t height);

    GpuHeightMap(const GpuHeightMap&) = delete;
    GpuHeightMap& operator=(const GpuHeightMap&) = delete;

    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }
    VkDeviceSize byteSize() const { return byteSize_; }

    VkBuffer buffer() const;
    VkDescriptorBufferInfo descriptorInfo() const;
    VkDescriptorBufferInfo readOnlyDescriptorInfo() const;
    wgen::HeightMap<float> copyToCpu() const;

private:
    LveDevice* renderDevice_{nullptr};
    LveComputeDevice* computeDevice_{nullptr};
    std::size_t width_{};
    std::size_t height_{};
    VkDeviceSize byteSize_{};
    std::unique_ptr<LveBuffer> storageBuffer_{};
};

} // namespace lve
