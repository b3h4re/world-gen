#include "render_system_2d.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace lve {

struct PushConstantData2d {
    glm::mat4 projectionView{1.0F};
    glm::mat4 model{1.0F};
};

RenderSystem2d::RenderSystem2d(
        LveDevice &device,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout)
    : device_{device},
      hasGlobalSetLayout_{globalSetLayout != VK_NULL_HANDLE} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
}

RenderSystem2d::~RenderSystem2d() {
    pipeline_.reset();
    vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
}

void RenderSystem2d::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.size = sizeof(PushConstantData2d);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{};
    if (globalSetLayout != VK_NULL_HANDLE) {
        descriptorSetLayouts.push_back(globalSetLayout);
    }

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<std::uint32_t>(descriptorSetLayouts.size());
    layoutInfo.pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create 2D pipeline layout");
    }
}

void RenderSystem2d::createPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo config{};
    LvePipeline::defaultPipelineConfigInfo(config);
    LvePipeline::enableAlphaBlending(config);
    config.renderPass = renderPass;
    config.pipelineLayout = pipelineLayout_;
    pipeline_ = std::make_unique<LvePipeline>(
        device_,
        std::string{WORLD_GEN_SHADER_DIR} + "/2d/terrain.vert.spv",
        std::string{WORLD_GEN_SHADER_DIR} + "/2d/terrain.frag.spv",
        config);
}

void RenderSystem2d::render(
    VkCommandBuffer commandBuffer,
    const Camera2d &camera,
    const std::vector<GameObject2d> &objects,
    VkDescriptorSet globalDescriptorSet) const {
    pipeline_->bind(commandBuffer);

    if (hasGlobalSetLayout_ && globalDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &globalDescriptorSet,
            0,
            nullptr);
    }

    for (const auto &object : objects) {
        PushConstantData2d push{};
        push.projectionView = camera.projectionView();
        push.model = object.transform.mat4();
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(PushConstantData2d),
            &push);
        object.mesh->bind(commandBuffer);
        object.mesh->draw(commandBuffer);
    }
}

void RenderSystem2d::render(FrameInfo &frameInfo) const {
    render(frameInfo.commandBuffer, frameInfo.camera2d, frameInfo.objects2d, frameInfo.globalDescriptorSet);
}

} // namespace lve
