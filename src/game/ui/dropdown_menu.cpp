#include "dropdown_menu.hpp"

#include "device/lve_device.hpp"
#include "game/2d/rendering/mesh_2d.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace lve {

namespace {

constexpr UiRect menuButtonRect{-0.98F, -0.96F, -0.90F, -0.86F};
constexpr UiRect menuPanelRect{-0.98F, -0.85F, -0.55F, -0.35F};
constexpr float menuButtonHeight{0.08F};
constexpr float menuButtonMargin{0.02F};

std::shared_ptr<Mesh2d> makeRectMesh(LveDevice &device, const UiRect &rect, glm::vec3 color) {
    const std::vector<Vertex2d> vertices{
        {{rect.left, rect.top}, color},
        {{rect.right, rect.top}, color},
        {{rect.right, rect.bottom}, color},
        {{rect.left, rect.bottom}, color},
    };
    const std::vector<std::uint32_t> indices{0, 1, 2, 0, 2, 3};

    return std::make_shared<Mesh2d>(device, vertices, indices);
}

UiRect menuButtonEntryRect(std::size_t index) {
    const float top = menuPanelRect.top + menuButtonMargin +
        static_cast<float>(index) * (menuButtonHeight + menuButtonMargin);
    return {
        menuPanelRect.left + menuButtonMargin,
        top,
        menuPanelRect.right - menuButtonMargin,
        top + menuButtonHeight,
    };
}

} // namespace

DropdownMenu::DropdownMenu(LveDevice &device, std::vector<UiButton::Config> buttonConfigs)
    : triggerButton_{device, menuButtonRect} {
    menuObjects_.push_back({makeRectMesh(device, menuPanelRect, {0.18F, 0.18F, 0.20F}), {}});
    menuButtons_.reserve(buttonConfigs.size());

    for (std::size_t i = 0; i < buttonConfigs.size(); ++i) {
        menuButtons_.emplace_back(device, menuButtonEntryRect(i), std::move(buttonConfigs[i]));
    }
}

bool DropdownMenu::update(GLFWwindow *window, VkExtent2D extent, bool escapePressed) {
    if (escapePressed && open_) {
        open_ = false;
        return true;
    }

    const bool mouseButtonIsPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (mouseButtonIsPressed && !mouseButtonWasPressed_) {
        double cursorX{};
        double cursorY{};
        glfwGetCursorPos(window, &cursorX, &cursorY);

        const float normalizedX =
            2.0F * static_cast<float>(cursorX) / static_cast<float>(extent.width) - 1.0F;
        const float normalizedY =
            2.0F * static_cast<float>(cursorY) / static_cast<float>(extent.height) - 1.0F;

        if (triggerButton_.click(normalizedX, normalizedY)) {
            open_ = !open_;
            mouseButtonWasPressed_ = mouseButtonIsPressed;
            return true;
        }

        if (open_) {
            for (auto &button : menuButtons_) {
                if (button.click(normalizedX, normalizedY)) {
                    mouseButtonWasPressed_ = mouseButtonIsPressed;
                    return true;
                }
            }

            const bool clickedInsidePanel =
                normalizedX >= menuPanelRect.left &&
                normalizedX <= menuPanelRect.right &&
                normalizedY >= menuPanelRect.top &&
                normalizedY <= menuPanelRect.bottom;
            if (!clickedInsidePanel) {
                open_ = false;
            }
            mouseButtonWasPressed_ = mouseButtonIsPressed;
            return true;
        }
    }

    mouseButtonWasPressed_ = mouseButtonIsPressed;
    return false;
}

void DropdownMenu::render(VkCommandBuffer commandBuffer, const RenderSystem2d &renderSystem) const {
    triggerButton_.render(commandBuffer, renderSystem, camera_);
    if (open_) {
        renderSystem.render(commandBuffer, camera_, menuObjects_);
        for (const auto &button : menuButtons_) {
            button.render(commandBuffer, renderSystem, camera_);
        }
    }
}

} // namespace lve
