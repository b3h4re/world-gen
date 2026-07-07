#pragma once

#include "device/lve_device.hpp"
#include "game/camera/camera_3d.hpp"
#include "game/objects/game_object_3d.hpp"
#include "pipeline/lve_pipeline.hpp"
#include "renderer/lve_frame_info.hpp"

#include <memory>
#include <vector>

namespace lve {

class RenderSystemPlanet {
public:
    RenderSystemPlanet(
        LveDevice &device,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout = VK_NULL_HANDLE);
    ~RenderSystemPlanet();

    RenderSystemPlanet(const RenderSystemPlanet &) = delete;
    RenderSystemPlanet &operator=(const RenderSystemPlanet &) = delete;

    void render(
        VkCommandBuffer commandBuffer,
        const Camera3d &camera,
        const std::vector<GameObject3d> &objects,
        VkDescriptorSet globalDescriptorSet = VK_NULL_HANDLE) const;
    void render(FrameInfo &frameInfo) const;

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    LveDevice &device_;
    VkPipelineLayout pipelineLayout_{};
    bool hasGlobalSetLayout_{false};
    std::unique_ptr<LvePipeline> pipeline_;
};

} // namespace lve
