#include "render_system_3d.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace lve {

struct PushConstantData3d {
    glm::mat4 projectionView{1.0F};
    glm::mat4 model{1.0F};
};

RenderSystem3d::RenderSystem3d(
        LveDevice &device,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout)
    : device_{device},
      hasGlobalSetLayout_{globalSetLayout != VK_NULL_HANDLE} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
}

RenderSystem3d::~RenderSystem3d() {
    pipeline_.reset();
    vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
}

void RenderSystem3d::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.size = sizeof(PushConstantData3d);

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
        throw std::runtime_error("failed to create 3D pipeline layout");
    }
}

void RenderSystem3d::createPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo config{};
    LvePipeline::defaultPipelineConfigInfo(config);
    config.bindingDescriptions = Vertex3d::getBindingDescriptions();
    config.attributeDescriptions = Vertex3d::getAttributeDescriptions();
    config.depthStencilInfo.depthTestEnable = VK_TRUE;
    config.depthStencilInfo.depthWriteEnable = VK_TRUE;
    config.renderPass = renderPass;
    config.pipelineLayout = pipelineLayout_;
    pipeline_ = std::make_unique<LvePipeline>(
        device_,
        std::string{WORLD_GEN_SHADER_DIR} + "/3d/terrain.vert.spv",
        std::string{WORLD_GEN_SHADER_DIR} + "/3d/terrain.frag.spv",
        config);
}

void RenderSystem3d::render(
    VkCommandBuffer commandBuffer,
    const Camera3d &camera,
    const std::vector<GameObject3d> &objects,
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
        PushConstantData3d push{};
        push.projectionView = camera.projectionView();
        push.model = object.transform.mat4();
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(PushConstantData3d),
            &push);
        object.mesh->bind(commandBuffer);
        object.mesh->draw(commandBuffer);
    }
}

void RenderSystem3d::render(FrameInfo &frameInfo) const {
    render(frameInfo.commandBuffer, frameInfo.camera3d, frameInfo.objects3d, frameInfo.globalDescriptorSet);
}

} // namespace lve
