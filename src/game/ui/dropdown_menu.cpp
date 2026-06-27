#include "dropdown_menu.hpp"

#include "device/lve_device.hpp"
#include "renderer/objects/mesh_2d.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>

namespace lve {

namespace {

constexpr UiRect menuButtonRect{-0.98F, -0.96F, -0.90F, -0.86F};
constexpr UiRect menuPanelRect{-0.98F, -0.85F, -0.55F, -0.35F};
constexpr float menuButtonHeight{0.08F};
constexpr float menuButtonMargin{0.02F};
constexpr float maxUiPixelHeight{720.0F};
constexpr float maxUiPixelWidth{1280.0F};
constexpr glm::vec2 layoutAnchor{-1.0F, -1.0F};

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
    const float top = menuPanelRect.top + menuButtonMargin + static_cast<float>(index) * (menuButtonHeight + menuButtonMargin);
    return {
        menuPanelRect.left + menuButtonMargin,
        top,
        menuPanelRect.right - menuButtonMargin,
        top + menuButtonHeight,
    };
}

} // namespace

DropdownMenu::DropdownMenu(LveDevice &device, const FontAtlas &font, std::vector<UiButton::Config> buttonConfigs)
    : triggerButton_{device, font, menuButtonRect, UiButton::Config{.text = "Menu"}} {
    menuObjects_.push_back({makeRectMesh(device, menuPanelRect, {0.18F, 0.18F, 0.20F}), {}});
    menuButtons_.reserve(buttonConfigs.size());

    for (std::size_t i = 0; i < buttonConfigs.size(); ++i) {
        menuButtons_.emplace_back(device, font, menuButtonEntryRect(i), std::move(buttonConfigs[i]));
    }
    updateLayout();
}

bool DropdownMenu::update(const AppInputState &input) {
    if (input.escapeJustPressed && open_) {
        open_ = false;
        return true;
    }

    if (input.primaryMouseJustPressed) {
        const glm::vec2 mouse = mouseToLocalUi(input.normalizedMouseX, input.normalizedMouseY);
        if (triggerButton_.click(mouse.x, mouse.y)) {
            open_ = !open_;
            return true;
        }

        if (open_) {
            for (auto &button : menuButtons_) {
                if (button.click(mouse.x, mouse.y)) {
                    return true;
                }
            }

            const bool clickedInsidePanel =
                mouse.x >= menuPanelRect.left && mouse.x <= menuPanelRect.right &&
                mouse.y >= menuPanelRect.top && mouse.y <= menuPanelRect.bottom;
            if (!clickedInsidePanel) {
                open_ = false;
            }
            return true;
        }
    }

    return false;
}

void DropdownMenu::setViewportExtent(VkExtent2D extent) {
    viewportExtent_ = extent;
    updateLayout();
}

glm::vec2 DropdownMenu::mouseToLocalUi(float normalizedMouseX, float normalizedMouseY) const {
    return (glm::vec2{normalizedMouseX, normalizedMouseY} - layoutTranslation_) / layoutScale_;
}

void DropdownMenu::updateLayout() {
    if (viewportExtent_.width == 0 || viewportExtent_.height == 0) {
        return;
    }

    const float widthScale = maxUiPixelWidth / static_cast<float>(viewportExtent_.width);
    const float heightScale = maxUiPixelHeight / static_cast<float>(viewportExtent_.height);
    layoutScale_ = std::min(1.0F, std::min(widthScale, heightScale));
    layoutTranslation_ = layoutAnchor - layoutScale_ * layoutAnchor;
}

void DropdownMenu::render(VkCommandBuffer commandBuffer, const RenderSystem2d &renderSystem, const TextRenderSystem &textRenderSystem) const {
    triggerButton_.render(commandBuffer, renderSystem, textRenderSystem, camera_, layoutScale_, layoutTranslation_);
    if (open_) {
        std::vector<GameObject2d> menuObjects = menuObjects_;
        for (auto &object : menuObjects) {
            object.transform.translation = layoutTranslation_ + layoutScale_ * object.transform.translation;
            object.transform.scale *= layoutScale_;
        }
        renderSystem.render(commandBuffer, camera_, menuObjects);
        for (const auto &button : menuButtons_) {
            button.render(commandBuffer, renderSystem, textRenderSystem, camera_, layoutScale_, layoutTranslation_);
        }
    }
}

} // namespace lve
