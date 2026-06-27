#include "terrain_render_system.hpp"

namespace lve {

TerrainRenderSystem::TerrainRenderSystem(LveDevice &device, VkRenderPass renderPass)
    : renderSystem2d_{device, renderPass},
      renderSystem3d_{device, renderPass} {}

void TerrainRenderSystem::render(
    VkCommandBuffer commandBuffer,
    bool render3d,
    const Camera2d &camera2d,
    const Camera3d &camera3d,
    const std::vector<GameObject2d> &objects2d,
    const std::vector<GameObject3d> &objects3d) const {
    if (render3d) {
        renderSystem3d_.render(commandBuffer, camera3d, objects3d);
        return;
    }

    renderSystem2d_.render(commandBuffer, camera2d, objects2d);
}

} // namespace lve
