#include "ui_button.hpp"

#include "device/lve_device.hpp"
#include "renderer/objects/mesh_2d.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace lve {

namespace {

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

} // namespace

UiButton::UiButton(LveDevice &device, const FontAtlas &font, UiRect rect)
    : UiButton{device, font, rect, Config{}} {}

UiButton::UiButton(LveDevice &device, const FontAtlas &font, UiRect rect, Config config)
    : rect_{rect}
    , text_{std::move(config.text)}
    , textColor_{config.textColor}
    , textScale_{config.textScale}
    , onClick_{std::move(config.onClick)} {
    objects_.push_back({makeRectMesh(device, rect_, config.color), {}});
    if (!text_.empty()) {
        auto mesh = std::make_shared<TextMesh>(device, font, text_, textColor_, textScale_);
        GameObjectText textObject{std::move(mesh), {}};
        textObject.transform.translation = {rect_.left + 0.01F, rect_.top + 0.02F};
        textObjects_.push_back(std::move(textObject));
    }
}

bool UiButton::click(float normalizedX, float normalizedY) {
    if (!contains(normalizedX, normalizedY)) {
        return false;
    }

    if (onClick_) {
        onClick_();
    }
    return true;
}

void UiButton::render(VkCommandBuffer commandBuffer, const RenderSystem2d &renderSystem,
    const TextRenderSystem &textRenderSystem, const Camera2d &camera) const {
    renderSystem.render(commandBuffer, camera, objects_);
    textRenderSystem.render(commandBuffer, camera, textObjects_);
}

bool UiButton::contains(float normalizedX, float normalizedY) const {
    return normalizedX >= rect_.left && normalizedX <= rect_.right && normalizedY >= rect_.top &&
           normalizedY <= rect_.bottom;
}

} // namespace lve
