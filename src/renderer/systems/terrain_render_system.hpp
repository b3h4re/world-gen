#pragma once

#include "device/lve_device.hpp"
#include "game/2d/camera/camera_2d.hpp"
#include "game/2d/objects/game_object_2d.hpp"
#include "render_system_2d.hpp"
#include "game/3d/camera/camera_3d.hpp"
#include "game/3d/objects/game_object_3d.hpp"
#include "render_system_3d.hpp"

#include <vulkan/vulkan.h>

#include <vector>

namespace lve {

class TerrainRenderSystem {
public:
    TerrainRenderSystem(LveDevice &device, VkRenderPass renderPass);

    TerrainRenderSystem(const TerrainRenderSystem &) = delete;
    TerrainRenderSystem &operator=(const TerrainRenderSystem &) = delete;

    void render(
        VkCommandBuffer commandBuffer,
        bool render3d,
        const Camera2d &camera2d,
        const Camera3d &camera3d,
        const std::vector<GameObject2d> &objects2d,
        const std::vector<GameObject3d> &objects3d) const;

    const RenderSystem2d &renderSystem2d() const { return renderSystem2d_; }

private:
    RenderSystem2d renderSystem2d_;
    RenderSystem3d renderSystem3d_;
};

} // namespace lve
