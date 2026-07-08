#include "loading_overlay_system.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace lve {

namespace {

struct OverlayPushConstantData {
    glm::mat4 model{1.0F};
    glm::vec4 color{1.0F};
};

std::vector<Vertex2d> loadingQuadVertices() {
    return {
        {{-0.5F, -0.5F}, 0.0F},
        {{ 0.5F, -0.5F}, 0.0F},
        {{ 0.5F,  0.5F}, 0.0F},
        {{-0.5F,  0.5F}, 0.0F},
    };
}

std::vector<std::uint32_t> quadIndices() {
    return {0, 1, 2, 0, 2, 3};
}

std::vector<Vertex3d> loadingPlanetVertices() {
    constexpr float r = 1.0F;
    return {
        {{ 0.0F,  r,    0.0F}, 1.0F},
        {{-r,     0.0F, 0.0F}, 0.35F},
        {{ 0.0F,  0.0F, r},    0.75F},
        {{ r,     0.0F, 0.0F}, 0.55F},
        {{ 0.0F,  0.0F, -r},   0.2F},
        {{ 0.0F, -r,    0.0F}, 0.0F},
    };
}

std::vector<std::uint32_t> loadingPlanetIndices() {
    return {
        0, 2, 3,
        0, 3, 4,
        0, 4, 1,
        0, 1, 2,
        5, 3, 2,
        5, 4, 3,
        5, 1, 4,
        5, 2, 1,
    };
}

glm::mat4 transform(glm::vec3 translation, glm::vec3 scale, glm::vec3 rotation = {}) {
    glm::mat4 result{1.0F};
    result = glm::translate(result, translation);
    result = glm::rotate(result, rotation.y, {0.0F, 1.0F, 0.0F});
    result = glm::rotate(result, rotation.x, {1.0F, 0.0F, 0.0F});
    result = glm::rotate(result, rotation.z, {0.0F, 0.0F, 1.0F});
    return glm::scale(result, scale);
}

} // namespace

LoadingOverlaySystem::LoadingOverlaySystem(
        LveDevice& device,
        VkRenderPass renderPass,
        LveDescriptorPool& descriptorPool)
    : device_{device},
      fontAtlas_{bakeFontAtlas(std::filesystem::path{"assets/fonts/Inter-Regular.ttf"}, 24.0F)},
      textRenderSystem_{device_, renderPass, descriptorPool, fontAtlas_},
      quadMesh_{std::make_shared<Mesh2d>(device_, loadingQuadVertices(), quadIndices())},
      planetMesh_{std::make_shared<Mesh3d>(device_, loadingPlanetVertices(), loadingPlanetIndices())} {
    auto textMesh = std::make_shared<TextMesh>(
        device_,
        fontAtlas_,
        "loading...",
        glm::vec3{0.94F, 0.97F, 1.0F},
        0.0042F);
    textObjects_.push_back({
        std::move(textMesh),
        Transform2d{
            .translation = {-0.115F, -0.044F},
        }
    });

    createQuadPipelineLayout();
    createPlanetPipelineLayout();
    createQuadPipeline(renderPass);
    createPlanetPipeline(renderPass);
}

LoadingOverlaySystem::~LoadingOverlaySystem() {
    quadPipeline_.reset();
    planetPipeline_.reset();

    if (quadPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_.device(), quadPipelineLayout_, nullptr);
    }
    if (planetPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_.device(), planetPipelineLayout_, nullptr);
    }
}

void LoadingOverlaySystem::render(VkCommandBuffer commandBuffer, float frameTime) {
    planetRotation_ += frameTime * 2.4F;

    renderQuad(commandBuffer);
    renderPlanet(commandBuffer);
    textRenderSystem_.render(commandBuffer, glm::mat4{1.0F}, textObjects_);
}

void LoadingOverlaySystem::createQuadPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(OverlayPushConstantData);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &quadPipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create loading overlay quad pipeline layout");
    }
}

void LoadingOverlaySystem::createPlanetPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(OverlayPushConstantData);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &planetPipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create loading overlay planet pipeline layout");
    }
}

void LoadingOverlaySystem::createQuadPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo config{};
    LvePipeline::defaultPipelineConfigInfo(config);
    LvePipeline::enableAlphaBlending(config);
    config.renderPass = renderPass;
    config.pipelineLayout = quadPipelineLayout_;
    quadPipeline_ = std::make_unique<LvePipeline>(
        device_,
        std::string{WORLD_GEN_SHADER_DIR} + "/loading/quad.vert.spv",
        std::string{WORLD_GEN_SHADER_DIR} + "/loading/quad.frag.spv",
        config);
}

void LoadingOverlaySystem::createPlanetPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo config{};
    LvePipeline::defaultPipelineConfigInfo(config);
    LvePipeline::enableAlphaBlending(config);
    config.bindingDescriptions = Vertex3d::getBindingDescriptions();
    config.attributeDescriptions = Vertex3d::getAttributeDescriptions();
    config.renderPass = renderPass;
    config.pipelineLayout = planetPipelineLayout_;
    planetPipeline_ = std::make_unique<LvePipeline>(
        device_,
        std::string{WORLD_GEN_SHADER_DIR} + "/loading/planet.vert.spv",
        std::string{WORLD_GEN_SHADER_DIR} + "/loading/planet.frag.spv",
        config);
}

void LoadingOverlaySystem::renderQuad(VkCommandBuffer commandBuffer) const {
    quadPipeline_->bind(commandBuffer);

    OverlayPushConstantData push{};
    push.model = transform({0.0F, 0.0F, 0.0F}, {0.48F, 0.15F, 1.0F});
    push.color = {0.02F, 0.025F, 0.035F, 0.72F};
    vkCmdPushConstants(
        commandBuffer,
        quadPipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(OverlayPushConstantData),
        &push);

    quadMesh_->bind(commandBuffer);
    quadMesh_->draw(commandBuffer);
}

void LoadingOverlaySystem::renderPlanet(VkCommandBuffer commandBuffer) const {
    planetPipeline_->bind(commandBuffer);

    OverlayPushConstantData push{};
    push.model = transform(
        {-0.18F, 0.0F, 0.0F},
        {0.042F, 0.042F, 0.042F},
        {0.25F, planetRotation_, -0.15F});
    push.color = {0.3F, 0.72F, 1.0F, 1.0F};
    vkCmdPushConstants(
        commandBuffer,
        planetPipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(OverlayPushConstantData),
        &push);

    planetMesh_->bind(commandBuffer);
    planetMesh_->draw(commandBuffer);
}

} // namespace lve
