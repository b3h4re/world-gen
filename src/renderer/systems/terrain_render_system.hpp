#pragma once

#include "device/lve_device.hpp"
#include "game/2d/camera/camera_2d.hpp"
#include "game/2d/objects/game_object_2d.hpp"
#include "render_system_2d.hpp"
#include "game/3d/camera/camera_3d.hpp"
#include "game/3d/objects/game_object_3d.hpp"
#include "render_system_3d.hpp"
#include "renderer/lve_frame_info.hpp"

#include <vulkan/vulkan.h>

#include <vector>

namespace lve {

class TerrainRenderSystem {
public:
    TerrainRenderSystem(LveDevice &device, VkRenderPass renderPass);

    TerrainRenderSystem(const TerrainRenderSystem &) = delete;
    TerrainRenderSystem &operator=(const TerrainRenderSystem &) = delete;

    void render(FrameInfo &frameInfo) const;

    const RenderSystem2d &renderSystem2d() const { return renderSystem2d_; }

private:
    RenderSystem2d renderSystem2d_;
    RenderSystem3d renderSystem3d_;
};

} // namespace lve
