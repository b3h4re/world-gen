#include "render_system_2d.hpp"

#include <stdexcept>

namespace lve {

struct PushConstantData2d {
    glm::mat4 projectionView{1.0F};
    glm::mat4 model{1.0F};
};

RenderSystem2d::RenderSystem2d(LveDevice &device, VkRenderPass renderPass)
    : device_{device} {
    createPipelineLayout();
    createPipeline(renderPass);
}

RenderSystem2d::~RenderSystem2d() {
    pipeline_.reset();
    vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
}

void RenderSystem2d::createPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.size = sizeof(PushConstantData2d);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
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
    const std::vector<GameObject2d> &objects) const {
    pipeline_->bind(commandBuffer);

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

} // namespace lve
