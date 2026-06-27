#pragma once

#include "game/2d/camera/camera_2d.hpp"
#include "game/2d/objects/game_object_2d.hpp"
#include "game/2d/rendering/render_system_2d.hpp"
#include "game/ui/ui_button.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>

namespace lve {

class LveDevice;

class DropdownMenu {
public:
    explicit DropdownMenu(LveDevice &device, std::vector<UiButton::Config> buttonConfigs = {});

    DropdownMenu(const DropdownMenu &) = delete;
    DropdownMenu &operator=(const DropdownMenu &) = delete;

    bool update(GLFWwindow *window, VkExtent2D extent, bool escapePressed);
    void render(VkCommandBuffer commandBuffer, const RenderSystem2d &renderSystem) const;

    bool isOpen() const { return open_; }

private:
    bool open_{false};
    bool mouseButtonWasPressed_{false};
    Camera2d camera_{};
    UiButton triggerButton_;
    std::vector<GameObject2d> menuObjects_;
    std::vector<UiButton> menuButtons_;
};

} // namespace lve
