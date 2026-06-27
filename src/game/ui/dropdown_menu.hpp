#pragma once

#include "game/camera/camera_2d.hpp"
#include "game/input/input_state.hpp"
#include "game/objects/game_object_2d.hpp"
#include "game/ui/ui_button.hpp"
#include "renderer/systems/render_system_2d.hpp"
#include "renderer/systems/text_render_system.hpp"

#include <vulkan/vulkan.h>

#include <vector>

namespace lve {

class LveDevice;

class DropdownMenu {
public:
    DropdownMenu(LveDevice &device, const FontAtlas &font, std::vector<UiButton::Config> buttonConfigs = {});

    DropdownMenu(const DropdownMenu &) = delete;
    DropdownMenu &operator=(const DropdownMenu &) = delete;

    bool update(const AppInputState &input);
    void render(VkCommandBuffer commandBuffer, const RenderSystem2d &renderSystem, const TextRenderSystem &textRenderSystem) const;

    bool isOpen() const { return open_; }

private:
    bool open_{false};
    Camera2d camera_{};
    UiButton triggerButton_;
    std::vector<GameObject2d> menuObjects_;
    std::vector<UiButton> menuButtons_;
};

} // namespace lve
