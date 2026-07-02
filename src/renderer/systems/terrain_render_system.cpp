#include "terrain_render_system.hpp"

namespace lve {

TerrainRenderSystem::TerrainRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : renderSystem2d_{device, renderPass, globalSetLayout},
      renderSystem3d_{device, renderPass, globalSetLayout} {}

void TerrainRenderSystem::render(FrameInfo &frameInfo) const {
    if (frameInfo.render3d) {
        renderSystem3d_.render(frameInfo);
        return;
    }

    renderSystem2d_.render(frameInfo);
}

} // namespace lve
