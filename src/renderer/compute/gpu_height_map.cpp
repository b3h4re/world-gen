#include "gpu_height_map.hpp"

#include <limits>
#include <stdexcept>

namespace lve {

namespace {

VkDeviceSize checkedByteSize(std::size_t width, std::size_t height) {
    if (width < 2 || height < 2) {
        throw std::invalid_argument("GPU heightmap dimensions must be at least 2x2");
    }

    if (width > std::numeric_limits<std::size_t>::max() / height) {
        throw std::overflow_error("GPU heightmap dimensions overflow size_t");
    }

    const std::size_t sampleCount = width * height;
    if (sampleCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("GPU heightmap sample count overflows uint32_t");
    }

    if (sampleCount > std::numeric_limits<VkDeviceSize>::max() / sizeof(float)) {
        throw std::overflow_error("GPU heightmap size overflows VkDeviceSize");
    }

    return static_cast<VkDeviceSize>(sampleCount * sizeof(float));
}

} // namespace

GpuHeightMap::GpuHeightMap(LveDevice& device, std::size_t width, std::size_t height)
    : renderDevice_{&device}, width_{width}, height_{height}, byteSize_{checkedByteSize(width, height)} {
    storageBuffer_ = std::make_unique<LveBuffer>(
        device,
        sizeof(float),
        static_cast<std::uint32_t>(width_ * height_),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

GpuHeightMap::GpuHeightMap(LveComputeDevice& device, std::size_t width, std::size_t height)
    : computeDevice_{&device}, width_{width}, height_{height}, byteSize_{checkedByteSize(width, height)} {
    storageBuffer_ = std::make_unique<LveBuffer>(
        device,
        sizeof(float),
        static_cast<std::uint32_t>(width_ * height_),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

VkBuffer GpuHeightMap::buffer() const {
    return storageBuffer_->getBuffer();
}

VkDescriptorBufferInfo GpuHeightMap::descriptorInfo() const {
    return VkDescriptorBufferInfo{
        storageBuffer_->getBuffer(),
        0,
        byteSize_,
    };
}

VkDescriptorBufferInfo GpuHeightMap::readOnlyDescriptorInfo() const {
    return descriptorInfo();
}

wgen::HeightMap<float> GpuHeightMap::copyToCpu() const {
    std::unique_ptr<LveBuffer> stagingBuffer;
    if (renderDevice_ != nullptr) {
        stagingBuffer = std::make_unique<LveBuffer>(
            *renderDevice_,
            byteSize_,
            1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        renderDevice_->copyBuffer(storageBuffer_->getBuffer(), stagingBuffer->getBuffer(), byteSize_);
    } else {
        stagingBuffer = std::make_unique<LveBuffer>(
            *computeDevice_,
            byteSize_,
            1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        computeDevice_->copyBuffer(storageBuffer_->getBuffer(), stagingBuffer->getBuffer(), byteSize_);
    }

    if (stagingBuffer->map() != VK_SUCCESS) {
        throw std::runtime_error("failed to map GPU heightmap readback buffer");
    }

    const auto* samples = static_cast<const float*>(stagingBuffer->getMappedMemory());
    wgen::HeightMap<float> heightMap{width_, height_};
    for (std::size_t y = 0; y < height_; ++y) {
        for (std::size_t x = 0; x < width_; ++x) {
            heightMap.at(x, y) = samples[y * width_ + x];
        }
    }

    return heightMap;
}

} // namespace lve
