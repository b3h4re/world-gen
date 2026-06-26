#pragma once

#include "device/lve_device.hpp"
#include "game/3d/camera/camera_3d.hpp"
#include "game/3d/objects/game_object_3d.hpp"
#include "pipeline/lve_pipeline.hpp"

#include <memory>
#include <vector>

namespace lve {

class RenderSystem3d {
public:
    RenderSystem3d(LveDevice &device, VkRenderPass renderPass);
    ~RenderSystem3d();

    RenderSystem3d(const RenderSystem3d &) = delete;
    RenderSystem3d &operator=(const RenderSystem3d &) = delete;

    void render(
        VkCommandBuffer commandBuffer,
        const Camera3d &camera,
        const std::vector<GameObject3d> &objects) const;

private:
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);

    LveDevice &device_;
    VkPipelineLayout pipelineLayout_{};
    std::unique_ptr<LvePipeline> pipeline_;
};

} // namespace lve
