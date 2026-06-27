#include "ui_button.hpp"

#include "device/lve_device.hpp"
#include "game/2d/rendering/mesh_2d.hpp"

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

UiButton::UiButton(LveDevice &device, UiRect rect)
    : UiButton{device, rect, Config{}} {}

UiButton::UiButton(LveDevice &device, UiRect rect, Config config)
    : rect_{rect}, onClick_{std::move(config.onClick)} {
    objects_.push_back({makeRectMesh(device, rect_, config.color), {}});
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

void UiButton::render(
    VkCommandBuffer commandBuffer,
    const RenderSystem2d &renderSystem,
    const Camera2d &camera) const {
    renderSystem.render(commandBuffer, camera, objects_);
}

bool UiButton::contains(float normalizedX, float normalizedY) const {
    return normalizedX >= rect_.left &&
        normalizedX <= rect_.right &&
        normalizedY >= rect_.top &&
        normalizedY <= rect_.bottom;
}

} // namespace lve
