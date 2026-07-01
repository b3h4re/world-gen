#include "value_noise_compute.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace lve {

namespace {

std::vector<char> readFile(const std::string& filepath) {
    std::ifstream file{filepath, std::ios::ate | std::ios::binary};
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filepath);
    }

    const auto fileSize = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    VkShaderModule shaderModule{VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create value noise compute shader module");
    }

    return shaderModule;
}

std::uint32_t checkedDimension(std::size_t value, const char* name) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error(std::string{name} + " overflows uint32_t");
    }

    return static_cast<std::uint32_t>(value);
}

std::uint32_t dispatchGroupCount(std::size_t size, std::uint32_t localSize) {
    return checkedDimension((size + localSize - 1) / localSize, "value noise dispatch group count");
}

} // namespace

VulkanValueNoiseGeneratorDevice::VulkanValueNoiseGeneratorDevice(LveDevice& device) : renderDevice_{&device} {
    if (!device.graphicsQueueSupportsCompute()) {
        throw std::runtime_error("selected Vulkan graphics queue does not support compute dispatch");
    }

    initialize();
}

VulkanValueNoiseGeneratorDevice::VulkanValueNoiseGeneratorDevice(LveComputeDevice& device) : computeDevice_{&device} {
    initialize();
}

void VulkanValueNoiseGeneratorDevice::initialize() {
    if (renderDevice_ != nullptr && !renderDevice_->supportsShaderInt64()) {
        throw std::runtime_error("selected Vulkan device does not support shaderInt64 required for CPU-matching value noise hash");
    }

    if (computeDevice_ != nullptr && !computeDevice_->supportsShaderInt64()) {
        throw std::runtime_error("selected Vulkan device does not support shaderInt64 required for CPU-matching value noise hash");
    }

    descriptorSetLayout_ = LveDescriptorSetLayout::Builder{vulkanDevice()}
        .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .build();

    descriptorPool_ = LveDescriptorPool::Builder{vulkanDevice()}
        .setMaxSets(64)
        .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64)
        .build();

    createPipelineLayout();
    createPipeline();
}

VulkanValueNoiseGeneratorDevice::~VulkanValueNoiseGeneratorDevice() {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(vulkanDevice(), pipeline_, nullptr);
    }

    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vulkanDevice(), pipelineLayout_, nullptr);
    }
}

VkDevice VulkanValueNoiseGeneratorDevice::vulkanDevice() const {
    if (renderDevice_ != nullptr) {
        return renderDevice_->device();
    }

    return computeDevice_->device();
}

VkCommandBuffer VulkanValueNoiseGeneratorDevice::beginSingleTimeCommands() {
    if (renderDevice_ != nullptr) {
        return renderDevice_->beginSingleTimeCommands();
    }

    return computeDevice_->beginSingleTimeCommands();
}

void VulkanValueNoiseGeneratorDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    if (renderDevice_ != nullptr) {
        renderDevice_->endSingleTimeCommands(commandBuffer);
        return;
    }

    computeDevice_->endSingleTimeCommands(commandBuffer);
}

std::unique_ptr<GpuHeightMap> VulkanValueNoiseGeneratorDevice::createGpuHeightMap(
    std::size_t width,
    std::size_t height) {
    if (renderDevice_ != nullptr) {
        return std::make_unique<GpuHeightMap>(*renderDevice_, width, height);
    }

    return std::make_unique<GpuHeightMap>(*computeDevice_, width, height);
}

std::unique_ptr<GpuHeightMap> VulkanValueNoiseGeneratorDevice::generateGpu(
    const wgen::ValueNoiseGeneratorSpec& spec,
    std::size_t width,
    std::size_t height) {
    auto heightMap = createGpuHeightMap(width, height);
    dispatch(spec, *heightMap);
    return heightMap;
}

std::unique_ptr<GpuHeightMap> VulkanValueNoiseGeneratorDevice::generateGpu(
    const wgen::ValueNoiseGenerator& generator,
    std::size_t width,
    std::size_t height) {
    return generateGpu(generator.spec().valueNoise, width, height);
}

wgen::HeightMap<float> VulkanValueNoiseGeneratorDevice::generateHeightMap(
    const wgen::ValueNoiseGenerator& generator,
    std::size_t width,
    std::size_t height) {
    return generateGpu(generator, width, height)->copyToCpu();
}

void VulkanValueNoiseGeneratorDevice::createPipelineLayout() {
    const VkDescriptorSetLayout descriptorSetLayout = descriptorSetLayout_->getDescriptorSetLayout();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(vulkanDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create value noise compute pipeline layout");
    }
}

void VulkanValueNoiseGeneratorDevice::createPipeline() {
    std::string file_path = std::string{WORLD_GEN_SHADER_DIR};
    file_path += "/compute/value_noise.comp.spv";
    const auto shaderCode = readFile(file_path);
    const VkShaderModule shaderModule = createShaderModule(vulkanDevice(), shaderCode);

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
        vulkanDevice(),
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &pipeline_
    );

    vkDestroyShaderModule(vulkanDevice(), shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create value noise compute pipeline");
    }
}

void VulkanValueNoiseGeneratorDevice::dispatch(const wgen::ValueNoiseGeneratorSpec& spec, GpuHeightMap& heightMap) {
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    VkDescriptorBufferInfo outputInfo = heightMap.descriptorInfo();
    if (!LveDescriptorWriter{*descriptorSetLayout_, *descriptorPool_}
             .writeBuffer(0, &outputInfo)
             .build(descriptorSet)) {
        throw std::runtime_error("failed to allocate value noise compute descriptor set");
    }

    const PushConstants pushConstants{
        .width = checkedDimension(heightMap.width(), "value noise width"),
        .height = checkedDimension(heightMap.height(), "value noise height"),
        .seed = spec.seed,
    };

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
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
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout_,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(PushConstants),
        &pushConstants
    );

    vkCmdDispatch(
        commandBuffer,
        dispatchGroupCount(heightMap.width(), LOCAL_SIZE_X),
        dispatchGroupCount(heightMap.height(), LOCAL_SIZE_Y),
        1
    );

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = heightMap.buffer();
    barrier.offset = 0;
    barrier.size = heightMap.byteSize();

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

    endSingleTimeCommands(commandBuffer);

    std::vector<VkDescriptorSet> descriptorSets{descriptorSet};
    descriptorPool_->freeDescriptors(descriptorSets);
}

} // namespace lve
