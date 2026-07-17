#pragma once

#include "device/lve_device.hpp"
#include "render_system_2d.hpp"
#include "render_system_3d.hpp"
#include "render_system_local_clipmap.hpp"
#include "render_system_planet.hpp"
#include "renderer/lve_frame_info.hpp"

namespace lve {

class TerrainRenderSystem {
public:
    TerrainRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout = VK_NULL_HANDLE);

    TerrainRenderSystem(const TerrainRenderSystem &) = delete;
    TerrainRenderSystem &operator=(const TerrainRenderSystem &) = delete;

    void render(FrameInfo &frameInfo) const;

    const RenderSystem2d &renderSystem2d() const { return renderSystem2d_; }

private:
    RenderSystem2d renderSystem2d_;
    RenderSystem3d renderSystem3d_;
    RenderSystemPlanet renderSystemPlanet_;
    RenderSystemLocalClipmap renderSystemLocalClipmap_;
};

} // namespace lve
