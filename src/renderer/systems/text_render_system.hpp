#pragma once

#include "device/lve_device.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"
#include "pipeline/lve_pipeline.hpp"
#include "renderer/lve_frame_info.hpp"
#include "stb/font_atlas.hpp"

#include <vulkan/vulkan.h>

#include <memory>

namespace lve {

class TextMesh;

class TextRenderSystem {
public:
    TextRenderSystem(LveDevice &device, VkRenderPass renderPass, LveDescriptorPool &descriptorPool,
                     const FontAtlas &fontAtlas);
    ~TextRenderSystem();

    TextRenderSystem(const TextRenderSystem &) = delete;
    TextRenderSystem &operator=(const TextRenderSystem &) = delete;

    void render(FrameInfo &frameInfo, const TextInfo &textInfo) const;
    void render(VkCommandBuffer commandBuffer, const Camera2d &camera, const TextInfo &textInfo) const;

private:
    void renderMesh(VkCommandBuffer commandBuffer, const Camera2d &camera, const TextMesh &textMesh) const;
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
