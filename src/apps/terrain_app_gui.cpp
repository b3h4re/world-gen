#include "terrain_app_gui.hpp"

#include "game/ui/ui_button.hpp"

#include <utility>

namespace lve {

TerrainAppGui::TerrainAppGui(LveDevice& device, Callbacks callbacks) : device_{device} {
    initFontFamily();
    initDropDownMenu(std::move(callbacks));
}

bool TerrainAppGui::update(const AppInputState& input) {
    return dropdownMenu_->update(input);
}

void TerrainAppGui::setViewportExtent(VkExtent2D extent) {
    dropdownMenu_->setViewportExtent(extent);
}

void TerrainAppGui::render(
        VkCommandBuffer commandBuffer,
        const RenderSystem2d& renderSystem,
        const TextRenderSystem& textRenderSystem) const {
    dropdownMenu_->render(commandBuffer, renderSystem, textRenderSystem);
}

const FontAtlas& TerrainAppGui::fontAtlasForPixelHeight(float pixelHeight) const {
    return fontFamily_.atlasForPixelHeight(pixelHeight);
}

void TerrainAppGui::initFontFamily() {
    constexpr float atlasSizes[] = {
        10.0F,
        16.0F,
        24.0F,
        32.0F,
        48.0F
    };
    fontFamily_ = FontFamily{"assets/fonts/Inter-Regular.ttf", atlasSizes};
}

void TerrainAppGui::initDropDownMenu(Callbacks callbacks) {
    UiButton::Config regenerateTerrainButton = {
        .color = {0.25F, 0.25F, 0.30F},
        .text = "Regenerate",
        .onClick = std::move(callbacks.regenerateTerrain),
    };

    UiButton::Config reloadTerrainButton = {
        .color = {0.25F, 0.25F, 0.30F},
        .text = "Reload seed",
        .onClick = std::move(callbacks.reloadTerrain),
    };

    UiButton::Config switchColorButton = {
        .color = {0.25F, 0.25F, 0.30F},
        .text = "Switch Color",
        .onClick = std::move(callbacks.switchColor),
    };

    std::vector<UiButton::Config> buttons{
        std::move(regenerateTerrainButton),
        std::move(reloadTerrainButton),
        std::move(switchColorButton)
    };

    dropdownMenu_ = std::make_unique<DropdownMenu>(device_, fontFamily_.atlasForPixelHeight(32.0F), std::move(buttons));
}

} // namespace lve
