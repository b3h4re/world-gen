#pragma once

#include "game/camera/camera_2d.hpp"
#include "game/objects/game_object_2d.hpp"
#include "game/objects/game_object_text.hpp"
#include "renderer/systems/render_system_2d.hpp"
#include "renderer/systems/text_render_system.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <functional>
#include <string>
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
        std::string text{};
        glm::vec3 textColor{1.0F, 1.0F, 1.0F};
        float textScale{0.002F};
        std::function<void()> onClick{};
    };

    UiButton(LveDevice &device, const FontAtlas &font, UiRect rect);
    UiButton(LveDevice &device, const FontAtlas &font, UiRect rect, Config config);

    bool click(float normalizedX, float normalizedY);
    void render(VkCommandBuffer commandBuffer, const RenderSystem2d &renderSystem,
                const TextRenderSystem &textRenderSystem, const Camera2d &camera) const;

    bool contains(float normalizedX, float normalizedY) const;
    const UiRect &rect() const { return rect_; }

private:
    UiRect rect_{};
    std::string text_{};
    glm::vec3 textColor_{1.0F, 1.0F, 1.0F};
    float textScale_{0.002F};
    std::function<void()> onClick_{};
    std::vector<GameObject2d> objects_;
    std::vector<GameObjectText> textObjects_;
};

} // namespace lve
