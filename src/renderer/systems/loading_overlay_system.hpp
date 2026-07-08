#pragma once

#include "device/lve_device.hpp"
#include "game/objects/game_object_text.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"
#include "pipeline/lve_pipeline.hpp"
#include "renderer/objects/mesh_2d.hpp"
#include "renderer/objects/mesh_3d.hpp"
#include "renderer/systems/text_render_system.hpp"
#include "stb/font_atlas.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace lve {

class LoadingOverlaySystem {
public:
    LoadingOverlaySystem(LveDevice& device, VkRenderPass renderPass, LveDescriptorPool& descriptorPool);
    ~LoadingOverlaySystem();

    LoadingOverlaySystem(const LoadingOverlaySystem&) = delete;
    LoadingOverlaySystem& operator=(const LoadingOverlaySystem&) = delete;

    void render(VkCommandBuffer commandBuffer, float frameTime);

private:
    void createQuadPipelineLayout();
    void createPlanetPipelineLayout();
    void createQuadPipeline(VkRenderPass renderPass);
    void createPlanetPipeline(VkRenderPass renderPass);
    void renderQuad(VkCommandBuffer commandBuffer) const;
    void renderPlanet(VkCommandBuffer commandBuffer) const;

    LveDevice& device_;
    FontAtlas fontAtlas_;
    TextRenderSystem textRenderSystem_;
    std::shared_ptr<Mesh2d> quadMesh_;
    std::shared_ptr<Mesh3d> planetMesh_;
    std::vector<GameObjectText> textObjects_;
    VkPipelineLayout quadPipelineLayout_{VK_NULL_HANDLE};
    VkPipelineLayout planetPipelineLayout_{VK_NULL_HANDLE};
    std::unique_ptr<LvePipeline> quadPipeline_;
    std::unique_ptr<LvePipeline> planetPipeline_;
    float planetRotation_{};
};

} // namespace lve
