#pragma once

#include "device/lve_compute_device.hpp"
#include "device/lve_device.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"
#include "renderer/compute/gpu_height_map.hpp"
#include "terrain/generators/noise/value_noise.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace lve {

class VulkanValueNoiseGeneratorDevice {
public:
    explicit VulkanValueNoiseGeneratorDevice(LveDevice& device);
    explicit VulkanValueNoiseGeneratorDevice(LveComputeDevice& device);
    ~VulkanValueNoiseGeneratorDevice();

    VulkanValueNoiseGeneratorDevice(const VulkanValueNoiseGeneratorDevice&) = delete;
    VulkanValueNoiseGeneratorDevice& operator=(const VulkanValueNoiseGeneratorDevice&) = delete;

    std::unique_ptr<GpuHeightMap> generateGpu(
        const wgen::ValueNoiseGeneratorSpec& spec,
        std::size_t width,
        std::size_t height);

    std::unique_ptr<GpuHeightMap> generateGpu(
        const wgen::ValueNoiseGenerator& generator,
        std::size_t width,
        std::size_t height);

    wgen::HeightMap<float> generateHeightMap(
        const wgen::ValueNoiseGenerator& generator,
        std::size_t width,
        std::size_t height);

private:
    struct PushConstants {
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint32_t seed{};
    };

    static constexpr std::uint32_t LOCAL_SIZE_X = 16;
    static constexpr std::uint32_t LOCAL_SIZE_Y = 16;

    void initialize();
    VkDevice vulkanDevice() const;
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    std::unique_ptr<GpuHeightMap> createGpuHeightMap(std::size_t width, std::size_t height);
    void createPipelineLayout();
    void createPipeline();
    void dispatch(const wgen::ValueNoiseGeneratorSpec& spec, GpuHeightMap& heightMap);

    LveDevice* renderDevice_{nullptr};
    LveComputeDevice* computeDevice_{nullptr};
    std::unique_ptr<LveDescriptorSetLayout> descriptorSetLayout_{};
    std::unique_ptr<LveDescriptorPool> descriptorPool_{};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
};

} // namespace lve
