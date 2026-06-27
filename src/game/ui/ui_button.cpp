#include "ui_button.hpp"

#include "device/lve_device.hpp"
#include "renderer/objects/mesh_2d.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace lve {

namespace {

constexpr float textPaddingX{0.01F};
constexpr float textPaddingY{0.01F};

struct TextBounds {
    glm::vec2 min{};
    glm::vec2 max{};
    bool hasVisibleGlyph{};

    glm::vec2 size() const { return max - min; }
    glm::vec2 center() const { return (min + max) * 0.5F; }
};

TextBounds scaleBounds(const TextBounds &bounds, float scale) {
    if (!bounds.hasVisibleGlyph) {
        return bounds;
    }

    return {
        bounds.min * scale,
        bounds.max * scale,
        true,
    };
}

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

TextBounds measureText(const FontAtlas &font, std::string_view text) {
    TextBounds bounds{};
    bounds.min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    bounds.max = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};

    glm::vec2 pen{};
    for (const char character : text) {
        if (character == '\n') {
            pen.x = 0.0F;
            pen.y += font.pixelHeight;
            continue;
        }

        const auto *glyph = font.glyph(static_cast<unsigned char>(character));
        if (glyph == nullptr) {
            continue;
        }

        const glm::vec2 glyphMin = pen + glyph->bearing;
        const glm::vec2 glyphMax = glyphMin + glyph->size;
        if (glyph->size.x > 0.0F && glyph->size.y > 0.0F) {
            bounds.min = glm::min(bounds.min, glyphMin);
            bounds.max = glm::max(bounds.max, glyphMax);
            bounds.hasVisibleGlyph = true;
        }

        pen.x += glyph->advance;
    }

    if (!bounds.hasVisibleGlyph) {
        bounds.min = {};
        bounds.max = {};
    }

    return bounds;
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
        const TextBounds unscaledBounds = measureText(font, text_);
        float fittedScale = textScale_;
        if (unscaledBounds.hasVisibleGlyph) {
            const glm::vec2 unscaledSize = unscaledBounds.size();
            const float availableWidth = std::max(0.0F, rect_.right - rect_.left - 2.0F * textPaddingX);
            const float availableHeight = std::max(0.0F, rect_.bottom - rect_.top - 2.0F * textPaddingY);
            float fitScale = textScale_;
            if (unscaledSize.x > 0.0F) {
                fitScale = std::min(fitScale, availableWidth / unscaledSize.x);
            }
            if (unscaledSize.y > 0.0F) {
                fitScale = std::min(fitScale, availableHeight / unscaledSize.y);
            }
            fittedScale = std::max(0.0F, fitScale);
        }

        auto mesh = std::make_shared<TextMesh>(device, font, text_, textColor_, fittedScale);
        GameObjectText textObject{std::move(mesh), {}};
        const TextBounds fittedBounds = scaleBounds(unscaledBounds, fittedScale);
        const glm::vec2 buttonCenter{(rect_.left + rect_.right) * 0.5F, (rect_.top + rect_.bottom) * 0.5F};
        textObject.transform.translation = buttonCenter - fittedBounds.center();
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
