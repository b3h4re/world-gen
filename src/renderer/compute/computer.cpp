#include "computer.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace lve {

std::vector<char> readFile(const std::string& filepath) {
    std::ifstream file{filepath, std::ios::ate | std::ios::binary};
    if (!file.is_open()) {
        throw std::runtime_error("failed to open compute shader: " + filepath);
    }

    const auto fileSize = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

std::string shaderPath(const std::string& shaderName) {
    std::string path = std::string{WORLD_GEN_SHADER_DIR} + "/compute/" + shaderName;
    if (!path.ends_with(".spv")) {
        path += ".comp.spv";
    }

    return path;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    VkShaderModule shaderModule{VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute shader module");
    }

    return shaderModule;
}

Computer::Computer(
        LveComputeDevice& device,
        std::string shaderName,
        std::size_t specSize,
        std::uint32_t storageBufferCount)
    : device_{device}, specSize_{specSize}, storageBufferCount_{storageBufferCount} {
    if (specSize_ > device_.properties.limits.maxPushConstantsSize) {
        throw std::runtime_error("compute spec is larger than max push constant size");
    }
    if (storageBufferCount_ == 0) {
        throw std::invalid_argument("compute pipeline must have at least one storage buffer");
    }

    LveDescriptorSetLayout::Builder setLayoutBuilder{device_};
    for (std::uint32_t binding = 0; binding < storageBufferCount_; ++binding) {
        setLayoutBuilder.addBinding(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    descriptorSetLayout_ = setLayoutBuilder.build();

    descriptorPool_ = LveDescriptorPool::Builder{device_}
        .setMaxSets(64)
        .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 * storageBufferCount_)
        .build();

    createPipelineLayout();
    createPipeline(shaderName);
}

Computer::~Computer() {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_.device(), pipeline_, nullptr);
    }

    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
    }
}

std::uint32_t Computer::dispatchGroupCount(std::size_t itemCount, std::uint32_t localSize) {
    if (localSize == 0) {
        throw std::invalid_argument("local compute workgroup size must be non-zero");
    }

    const std::size_t groupCount = (itemCount + localSize - 1) / localSize;
    if (groupCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("compute dispatch group count overflows uint32_t");
    }

    return static_cast<std::uint32_t>(groupCount);
}

void Computer::dispatch(
    const void* spec,
    std::size_t specSize,
    VkDescriptorBufferInfo outputBuffer,
    ComputeDispatchSize dispatchSize) const {
    dispatch(spec, specSize, std::span<const VkDescriptorBufferInfo>{&outputBuffer, 1}, dispatchSize);
}

void Computer::dispatch(
    const void* spec,
    std::size_t specSize,
    std::span<const VkDescriptorBufferInfo> storageBuffers,
    ComputeDispatchSize dispatchSize) const {
    if (specSize != specSize_) {
        throw std::invalid_argument("compute spec size does not match pipeline layout");
    }
    if (storageBuffers.size() != storageBufferCount_) {
        throw std::invalid_argument("compute storage buffer count does not match pipeline layout");
    }

    std::vector<VkDescriptorBufferInfo> bufferInfos{storageBuffers.begin(), storageBuffers.end()};
    LveDescriptorWriter writer{*descriptorSetLayout_, *descriptorPool_};
    for (std::uint32_t binding = 0; binding < storageBufferCount_; ++binding) {
        writer.writeBuffer(binding, &bufferInfos[binding]);
    }

    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    if (!writer.build(descriptorSet)) {
        throw std::runtime_error("failed to allocate compute descriptor set");
    }

    VkCommandBuffer commandBuffer = device_.beginSingleTimeCommands();
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipelineLayout_,
        0,
        1,
        &descriptorSet,
        0,
        nullptr
    );

    if (specSize_ > 0) {
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            static_cast<std::uint32_t>(specSize_),
            spec
        );
    }

    vkCmdDispatch(commandBuffer, dispatchSize.groupCountX, dispatchSize.groupCountY, dispatchSize.groupCountZ);

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = storageBuffers.back().buffer;
    barrier.offset = storageBuffers.back().offset;
    barrier.size = storageBuffers.back().range;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0,
        nullptr,
        1,
        &barrier,
        0,
        nullptr
    );

    device_.endSingleTimeCommands(commandBuffer);

    std::vector<VkDescriptorSet> descriptorSets{descriptorSet};
    descriptorPool_->freeDescriptors(descriptorSets);
}

void Computer::createPipelineLayout() {
    const VkDescriptorSetLayout descriptorSetLayout = descriptorSetLayout_->getDescriptorSetLayout();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = static_cast<std::uint32_t>(specSize_);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (specSize_ > 0) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout");
    }
}

void Computer::createPipeline(const std::string& shaderName) {
    const auto shaderCode = readFile(shaderPath(shaderName));
    const VkShaderModule shaderModule = createShaderModule(device_.device(), shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = pipelineLayout_;

    const VkResult result = vkCreateComputePipelines(
        device_.device(),
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &pipeline_
    );

    vkDestroyShaderModule(device_.device(), shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline");
    }
}

} // namespace lve
