#pragma once

#include "device/lve_compute_device.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>

namespace lve {

struct ComputeDispatchSize {
    std::uint32_t groupCountX{1};
    std::uint32_t groupCountY{1};
    std::uint32_t groupCountZ{1};
};

class Computer {
public:
    Computer(
        LveComputeDevice& device,
        std::string shaderName,
        std::size_t specSize,
        std::uint32_t storageBufferCount = 1);
    template <typename Spec>
    Computer(
        LveComputeDevice& device,
        std::string shaderName,
        const Spec&,
        std::uint32_t storageBufferCount = 1)
        : Computer{device, std::move(shaderName), sizeof(Spec), storageBufferCount} {}
    ~Computer();

    Computer(const Computer&) = delete;
    Computer& operator=(const Computer&) = delete;

    static std::uint32_t dispatchGroupCount(std::size_t itemCount, std::uint32_t localSize);

    void dispatch(
        const void* spec,
        std::size_t specSize,
        VkDescriptorBufferInfo outputBuffer,
        ComputeDispatchSize dispatchSize) const;
    void dispatch(
        const void* spec,
        std::size_t specSize,
        std::span<const VkDescriptorBufferInfo> storageBuffers,
        ComputeDispatchSize dispatchSize) const;

    template <typename Spec>
    void dispatch(
        const Spec& spec,
        VkDescriptorBufferInfo outputBuffer,
        ComputeDispatchSize dispatchSize) const {
        dispatch(&spec, sizeof(Spec), outputBuffer, dispatchSize);
    }
    template <typename Spec>
    void dispatch(
        const Spec& spec,
        std::span<const VkDescriptorBufferInfo> storageBuffers,
        ComputeDispatchSize dispatchSize) const {
        dispatch(&spec, sizeof(Spec), storageBuffers, dispatchSize);
    }

private:
    void createPipelineLayout();
    void createPipeline(const std::string& shaderName);

    LveComputeDevice& device_;
    std::size_t specSize_{};
    std::uint32_t storageBufferCount_{1};
    std::unique_ptr<LveDescriptorSetLayout> descriptorSetLayout_{};
    std::unique_ptr<LveDescriptorPool> descriptorPool_{};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
};

} // namespace lve
