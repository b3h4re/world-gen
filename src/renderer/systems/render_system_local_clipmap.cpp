#include "renderer/systems/render_system_local_clipmap.hpp"

#include "renderer/objects/local_clipmap_render_resources.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace lve {
namespace {

struct PushConstantDataLocalClipmap {
    glm::mat4 projectionView{1.0F};
    glm::vec3 relativeOrigin{};
    float cacheValidity{1.0F};
    glm::vec4 levelColor{1.0F};
};

static_assert(offsetof(PushConstantDataLocalClipmap, relativeOrigin) == 64);
static_assert(offsetof(PushConstantDataLocalClipmap, cacheValidity) == 76);
static_assert(offsetof(PushConstantDataLocalClipmap, levelColor) == 80);
static_assert(sizeof(PushConstantDataLocalClipmap) == 96);

glm::vec4 debugLevelColor(std::uint32_t level) {
    constexpr std::array<glm::vec4, 8> COLORS{
        glm::vec4{0.10F, 0.85F, 1.00F, 1.0F},
        glm::vec4{0.20F, 1.00F, 0.35F, 1.0F},
        glm::vec4{0.95F, 0.95F, 0.20F, 1.0F},
        glm::vec4{1.00F, 0.55F, 0.10F, 1.0F},
        glm::vec4{1.00F, 0.20F, 0.35F, 1.0F},
        glm::vec4{0.85F, 0.20F, 1.00F, 1.0F},
        glm::vec4{0.35F, 0.30F, 1.00F, 1.0F},
        glm::vec4{0.90F, 0.90F, 0.90F, 1.0F},
    };
    return COLORS[level % COLORS.size()];
}

void pushObject(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const Camera3d& camera,
        const LocalClipmapRenderObject& renderObject) {
    PushConstantDataLocalClipmap push{};
    push.projectionView = camera.renderProjectionView();
    push.relativeOrigin = camera.positionRelativeToRenderOrigin(
        renderObject.object.globalOrigin);
    push.cacheValidity = renderObject.cacheCurrent ? 1.0F : 0.0F;
    push.levelColor = debugLevelColor(renderObject.level);
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstantDataLocalClipmap),
        &push);
}

} // namespace

RenderSystemLocalClipmap::RenderSystemLocalClipmap(
        LveDevice& device,
        VkRenderPass renderPass)
    : device_{device} {
    createPipelineLayout();
    createPipelines(renderPass);
}

RenderSystemLocalClipmap::~RenderSystemLocalClipmap() {
    linePipeline_.reset();
    fillPipeline_.reset();
    vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
}

void RenderSystemLocalClipmap::render(FrameInfo& frameInfo) const {
    fillPipeline_->bind(frameInfo.commandBuffer);
    for (const LocalClipmapRenderObject& renderObject :
            frameInfo.objectsLocalClipmap) {
        pushObject(
            frameInfo.commandBuffer,
            pipelineLayout_,
            frameInfo.cameraPlanet,
            renderObject);
        renderObject.object.mesh->bind(
            frameInfo.commandBuffer,
            renderObject.object.meshIndexVariant);
        renderObject.object.mesh->draw(
            frameInfo.commandBuffer,
            renderObject.object.meshIndexVariant);
    }

    linePipeline_->bind(frameInfo.commandBuffer);
    for (const LocalClipmapRenderObject& renderObject :
            frameInfo.objectsLocalClipmap) {
        pushObject(
            frameInfo.commandBuffer,
            pipelineLayout_,
            frameInfo.cameraPlanet,
            renderObject);
        const std::size_t lineVariant = renderObject.level == 0
            ? LOCAL_CLIPMAP_CENTER_LINES_INDEX_VARIANT
            : LOCAL_CLIPMAP_RING_LINES_INDEX_VARIANT;
        renderObject.object.mesh->bind(frameInfo.commandBuffer, lineVariant);
        renderObject.object.mesh->draw(frameInfo.commandBuffer, lineVariant);
    }
}

void RenderSystemLocalClipmap::createPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.size = sizeof(PushConstantDataLocalClipmap);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(
            device_.device(),
            &layoutInfo,
            nullptr,
            &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error{
            "failed to create local clipmap pipeline layout"};
    }
}

void RenderSystemLocalClipmap::createPipelines(VkRenderPass renderPass) {
    PipelineConfigInfo fillConfig{};
    LvePipeline::defaultPipelineConfigInfo(fillConfig);
    fillConfig.bindingDescriptions = Vertex3d::getBindingDescriptions();
    fillConfig.attributeDescriptions = Vertex3d::getAttributeDescriptions();
    fillConfig.depthStencilInfo.depthTestEnable = VK_TRUE;
    fillConfig.depthStencilInfo.depthWriteEnable = VK_TRUE;
    fillConfig.renderPass = renderPass;
    fillConfig.pipelineLayout = pipelineLayout_;
    fillPipeline_ = std::make_unique<LvePipeline>(
        device_,
        std::string{WORLD_GEN_SHADER_DIR} + "/local_clipmap/terrain.vert.spv",
        std::string{WORLD_GEN_SHADER_DIR} + "/local_clipmap/terrain.frag.spv",
        fillConfig);

    PipelineConfigInfo lineConfig{};
    LvePipeline::defaultPipelineConfigInfo(lineConfig);
    lineConfig.bindingDescriptions = Vertex3d::getBindingDescriptions();
    lineConfig.attributeDescriptions = Vertex3d::getAttributeDescriptions();
    lineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    lineConfig.depthStencilInfo.depthTestEnable = VK_TRUE;
    lineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
    lineConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    lineConfig.renderPass = renderPass;
    lineConfig.pipelineLayout = pipelineLayout_;
    linePipeline_ = std::make_unique<LvePipeline>(
        device_,
        std::string{WORLD_GEN_SHADER_DIR} + "/local_clipmap/terrain.vert.spv",
        std::string{WORLD_GEN_SHADER_DIR} + "/local_clipmap/wire.frag.spv",
        lineConfig);
}

} // namespace lve
