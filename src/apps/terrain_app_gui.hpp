#pragma once

#include "device/lve_device.hpp"
#include "game/input/input_state.hpp"
#include "game/ui/dropdown_menu.hpp"
#include "renderer/systems/render_system_2d.hpp"
#include "renderer/systems/text_render_system.hpp"
#include "stb/font_atlas.hpp"

#include <functional>
#include <memory>
#include <vulkan/vulkan.h>

namespace lve {

class TerrainAppGui {
public:
    struct Callbacks {
        std::function<void()> regenerateTerrain;
        std::function<void()> reloadTerrain;
        std::function<void()> switchColor;
    };

    TerrainAppGui(LveDevice& device, Callbacks callbacks);

    TerrainAppGui(const TerrainAppGui&) = delete;
    TerrainAppGui& operator=(const TerrainAppGui&) = delete;

    bool update(const AppInputState& input);
    void setViewportExtent(VkExtent2D extent);
    void render(
        VkCommandBuffer commandBuffer,
        const RenderSystem2d& renderSystem,
        const TextRenderSystem& textRenderSystem) const;

    const FontAtlas& fontAtlasForPixelHeight(float pixelHeight) const;

private:
    void initFontFamily();
    void initDropDownMenu(Callbacks callbacks);

    LveDevice& device_;
    FontFamily fontFamily_{};
    std::unique_ptr<DropdownMenu> dropdownMenu_{};
};

} // namespace lve
