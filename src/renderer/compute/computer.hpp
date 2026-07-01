#pragma once

#include "device/lve_compute_device.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
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
    Computer(LveComputeDevice& device, std::string shaderName, std::size_t specSize);
    template <typename Spec>
    Computer(LveComputeDevice& device, std::string shaderName, const Spec&)
        : Computer{device, std::move(shaderName), sizeof(Spec)} {}
    ~Computer();

    Computer(const Computer&) = delete;
    Computer& operator=(const Computer&) = delete;

    static std::uint32_t dispatchGroupCount(std::size_t itemCount, std::uint32_t localSize);

    void dispatch(
        const void* spec,
        std::size_t specSize,
        VkDescriptorBufferInfo outputBuffer,
        ComputeDispatchSize dispatchSize) const;

    template <typename Spec>
    void dispatch(
        const Spec& spec,
        VkDescriptorBufferInfo outputBuffer,
        ComputeDispatchSize dispatchSize) const {
        dispatch(&spec, sizeof(Spec), outputBuffer, dispatchSize);
    }

private:
    void createPipelineLayout();
    void createPipeline(const std::string& shaderName);

    LveComputeDevice& device_;
    std::size_t specSize_{};
    std::unique_ptr<LveDescriptorSetLayout> descriptorSetLayout_{};
    std::unique_ptr<LveDescriptorPool> descriptorPool_{};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
};

} // namespace lve
