#pragma once

#include "device/lve_device.hpp"
#include "game/objects/game_object_text.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"
#include "pipeline/lve_pipeline.hpp"
#include "renderer/lve_frame_info.hpp"
#include "stb/font_atlas.hpp"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace lve {

class TextRenderSystem {
public:
    TextRenderSystem(LveDevice &device, VkRenderPass renderPass, LveDescriptorPool &descriptorPool,
                     const FontAtlas &fontAtlas);
    ~TextRenderSystem();

    TextRenderSystem(const TextRenderSystem &) = delete;
    TextRenderSystem &operator=(const TextRenderSystem &) = delete;

    const FontAtlas &fontAtlas() const { return fontAtlas_; }

    void render(FrameInfo &frameInfo, const std::vector<GameObjectText> &objects) const;
    void render(VkCommandBuffer commandBuffer, const Camera2d &camera,
                const std::vector<GameObjectText> &objects) const;

private:
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);
    void createFontTexture(const FontAtlas &fontAtlas);
    void createFontImageView();
    void createFontSampler();
    void createFontDescriptor();
    void transitionFontImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);

    LveDevice &device_;
    LveDescriptorPool &descriptorPool_;
    const FontAtlas &fontAtlas_;
    std::unique_ptr<LveDescriptorSetLayout> fontSetLayout_;
    VkDescriptorSet fontDescriptorSet_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{};
    std::unique_ptr<LvePipeline> pipeline_;
    std::uint32_t fontWidth_{};
    std::uint32_t fontHeight_{};
    VkImage fontImage_{VK_NULL_HANDLE};
    VkDeviceMemory fontImageMemory_{VK_NULL_HANDLE};
    VkImageView fontImageView_{VK_NULL_HANDLE};
    VkSampler fontSampler_{VK_NULL_HANDLE};
};

} // namespace lve
