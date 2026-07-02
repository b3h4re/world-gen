#pragma once

#include "device/lve_device.hpp"
#include "game/camera/camera_2d.hpp"
#include "game/objects/game_object_2d.hpp"
#include "pipeline/lve_pipeline.hpp"
#include "renderer/lve_frame_info.hpp"

#include <memory>
#include <vector>

namespace lve {

class RenderSystem2d {
public:
    RenderSystem2d(
        LveDevice &device,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout = VK_NULL_HANDLE);
    ~RenderSystem2d();

    RenderSystem2d(const RenderSystem2d &) = delete;
    RenderSystem2d &operator=(const RenderSystem2d &) = delete;

    void render(
        VkCommandBuffer commandBuffer,
        const Camera2d &camera,
        const std::vector<GameObject2d> &objects,
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
