#include "loading_overlay_system.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <tiny_obj_loader.h>

#include <filesystem>
#include <string>
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

struct LoadingPlanetMeshData {
    std::vector<Vertex3d> vertices;
    std::vector<std::uint32_t> indices;
};

LoadingPlanetMeshData loadLoadingPlanetMesh(const std::filesystem::path& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.string().c_str(), nullptr, true)) {
        throw std::runtime_error("failed to load loading planet mesh: " + warn + err);
    }
    if (attrib.vertices.empty() || shapes.empty()) {
        throw std::runtime_error("loading planet mesh is empty: " + filepath.string());
    }

    std::vector<Vertex3d> vertices;
    vertices.reserve(attrib.vertices.size() / 3);
    for (std::size_t i = 0; i + 2 < attrib.vertices.size(); i += 3) {
        const glm::vec3 position{
            attrib.vertices[i],
            attrib.vertices[i + 1],
            attrib.vertices[i + 2],
        };
        vertices.push_back({
            .position = position,
            .height = (glm::length(position) - 1.0F) * 10.0F,
        });
    }

    std::vector<std::uint32_t> indices;
    for (const tinyobj::shape_t& shape : shapes) {
        indices.reserve(indices.size() + shape.mesh.indices.size());
        for (const tinyobj::index_t& index : shape.mesh.indices) {
            if (index.vertex_index < 0) {
                throw std::runtime_error("loading planet mesh contains a face without a vertex index");
            }
            indices.push_back(static_cast<std::uint32_t>(index.vertex_index));
        }
    }

    if (indices.empty()) {
        throw std::runtime_error("loading planet mesh has no indices: " + filepath.string());
    }

    return {
        std::move(vertices),
        std::move(indices),
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
      quadMesh_{std::make_shared<Mesh2d>(device_, loadingQuadVertices(), quadIndices())} {
    LoadingPlanetMeshData planetData = loadLoadingPlanetMesh("assets/models/loading_planet.obj");
    planetMesh_ = std::make_shared<Mesh3d>(device_, planetData.vertices, planetData.indices);

    auto textMesh = std::make_shared<TextMesh>(
        device_,
        fontAtlas_,
        "loading...",
        glm::vec3{0.94F, 0.97F, 1.0F},
        0.0042F);
    textObjects_.push_back({
        std::move(textMesh),
        Transform2d{
            .translation = {-0.12F, 0.021F},
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
