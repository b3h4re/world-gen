#include "terrain_render_system.hpp"

namespace lve {

TerrainRenderSystem::TerrainRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : renderSystem2d_{device, renderPass, globalSetLayout},
      renderSystem3d_{device, renderPass, globalSetLayout},
      renderSystemPlanet_{device, renderPass, globalSetLayout},
      renderSystemLocalClipmap_{device, renderPass, globalSetLayout} {}

void TerrainRenderSystem::render(FrameInfo &frameInfo) const {
    switch (frameInfo.renderMode) {
        case TerrainRenderModes::FlatPicture:
            renderSystem2d_.render(frameInfo);
            return;
        case TerrainRenderModes::PlaneMesh3D:
            renderSystem3d_.render(frameInfo);
            return;
        case TerrainRenderModes::PlanetView:
            renderSystemPlanet_.render(frameInfo);
            if (frameInfo.localClipmapCoverageActive) {
                renderSystemLocalClipmap_.render(frameInfo);
            }
            return;
        case TerrainRenderModes::LocalClipmapDebug:
            renderSystemLocalClipmap_.render(frameInfo);
            return;
    }
}

} // namespace lve
