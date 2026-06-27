#pragma once

#include "game/2d/camera/camera_2d.hpp"
#include "game/2d/objects/game_object_2d.hpp"
#include "game/2d/rendering/render_system_2d.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <functional>
#include <vector>

namespace lve {

class LveDevice;

struct UiRect {
    float left;
    float top;
    float right;
    float bottom;
};

class UiButton {
public:
    struct Config {
        glm::vec3 color{0.12F, 0.12F, 0.14F};
        std::function<void()> onClick{};
    };

    UiButton(LveDevice &device, UiRect rect);
    UiButton(LveDevice &device, UiRect rect, Config config);

    bool click(float normalizedX, float normalizedY);
    void render(VkCommandBuffer commandBuffer, const RenderSystem2d &renderSystem, const Camera2d &camera) const;

    bool contains(float normalizedX, float normalizedY) const;
    const UiRect &rect() const { return rect_; }

private:
    UiRect rect_{};
    std::function<void()> onClick_{};
    std::vector<GameObject2d> objects_;
};

} // namespace lve
