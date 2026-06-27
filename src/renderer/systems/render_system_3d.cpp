#include "render_system_3d.hpp"

#include <stdexcept>

namespace lve {

struct PushConstantData3d {
    glm::mat4 projectionView{1.0F};
    glm::mat4 model{1.0F};
};

RenderSystem3d::RenderSystem3d(LveDevice &device, VkRenderPass renderPass)
    : device_{device} {
    createPipelineLayout();
    createPipeline(renderPass);
}

RenderSystem3d::~RenderSystem3d() {
    pipeline_.reset();
    vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
}

void RenderSystem3d::createPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.size = sizeof(PushConstantData3d);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
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
    const std::vector<GameObject3d> &objects) const {
    pipeline_->bind(commandBuffer);

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

} // namespace lve
