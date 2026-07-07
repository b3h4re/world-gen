#include "render_system_planet.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace lve {

struct PushConstantDataPlanet {
    glm::mat4 projectionView{1.0F};
    glm::mat4 model{1.0F};
};

RenderSystemPlanet::RenderSystemPlanet(
        LveDevice &device,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout)
    : device_{device},
      hasGlobalSetLayout_{globalSetLayout != VK_NULL_HANDLE} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
}

RenderSystemPlanet::~RenderSystemPlanet() {
    pipeline_.reset();
    vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
}

void RenderSystemPlanet::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.size = sizeof(PushConstantDataPlanet);

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

void RenderSystemPlanet::createPipeline(VkRenderPass renderPass) {
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
        std::string{WORLD_GEN_SHADER_DIR} + "/planet/terrain.vert.spv",
        std::string{WORLD_GEN_SHADER_DIR} + "/planet/terrain.frag.spv",
        config);
}

void RenderSystemPlanet::render(
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
        PushConstantDataPlanet push{};
        push.projectionView = camera.projectionView();
        push.model = object.transform.mat4();
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(PushConstantDataPlanet),
            &push);
        object.mesh->bind(commandBuffer);
        object.mesh->draw(commandBuffer);
    }
}

void RenderSystemPlanet::render(FrameInfo &frameInfo) const {
    render(frameInfo.commandBuffer, frameInfo.cameraPlanet, frameInfo.objectsPlanet, frameInfo.globalDescriptorSet);
}

} // namespace lve
