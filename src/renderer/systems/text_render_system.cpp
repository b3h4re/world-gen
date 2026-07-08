#include "text_render_system.hpp"

#include "model/buffer/lve_buffer.hpp"
#include "renderer/objects/text_mesh.hpp"

#include <glm/glm.hpp>

#include <stdexcept>

namespace lve {

namespace {

struct TextPushConstantData {
    glm::mat4 projectionView{1.0F};
    glm::mat4 model{1.0F};
};

} // namespace

TextRenderSystem::TextRenderSystem(LveDevice &device, VkRenderPass renderPass, LveDescriptorPool &descriptorPool,
                                   const FontAtlas &fontAtlas)
    : device_{device}
    , descriptorPool_{descriptorPool}
    , fontAtlas_{fontAtlas} {
    fontSetLayout_ = LveDescriptorSetLayout::Builder(device_)
                         .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                         .build();
    createFontTexture(fontAtlas);
    createFontImageView();
    createFontSampler();
    createFontDescriptor();
    createPipelineLayout();
    createPipeline(renderPass);
}

TextRenderSystem::~TextRenderSystem() {
    pipeline_.reset();
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
    }
    if (fontSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_.device(), fontSampler_, nullptr);
    }
    if (fontImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_.device(), fontImageView_, nullptr);
    }
    if (fontImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_.device(), fontImage_, nullptr);
    }
    if (fontImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_.device(), fontImageMemory_, nullptr);
    }
}

void TextRenderSystem::createPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(TextPushConstantData);

    const VkDescriptorSetLayout descriptorSetLayout = fontSetLayout_->getDescriptorSetLayout();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create text pipeline layout");
    }
}

void TextRenderSystem::createPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo config{};
    LvePipeline::defaultPipelineConfigInfo(config);
    LvePipeline::enableAlphaBlending(config);
    config.bindingDescriptions = TextVertex::getBindingDescriptions();
    config.attributeDescriptions = TextVertex::getAttributeDescriptions();
    config.renderPass = renderPass;
    config.pipelineLayout = pipelineLayout_;
    pipeline_ = std::make_unique<LvePipeline>(device_, std::string{WORLD_GEN_SHADER_DIR} + "/text/text.vert.spv",
                                              std::string{WORLD_GEN_SHADER_DIR} + "/text/text.frag.spv", config);
}

void TextRenderSystem::createFontTexture(const FontAtlas &fontAtlas) {
    if (fontAtlas.width <= 0 || fontAtlas.height <= 0 || fontAtlas.pixels.empty()) {
        throw std::runtime_error("cannot create text renderer from an empty font atlas");
    }

    fontWidth_ = static_cast<std::uint32_t>(fontAtlas.width);
    fontHeight_ = static_cast<std::uint32_t>(fontAtlas.height);
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(fontAtlas.pixels.size());

    LveBuffer stagingBuffer{device_, imageSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    stagingBuffer.map();
    stagingBuffer.writeToBuffer(const_cast<unsigned char *>(fontAtlas.pixels.data()), imageSize);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = fontWidth_;
    imageInfo.extent.height = fontHeight_;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    device_.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fontImage_, fontImageMemory_);

    transitionFontImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    device_.copyBufferToImage(stagingBuffer.getBuffer(), fontImage_, fontWidth_, fontHeight_, 1);
    transitionFontImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextRenderSystem::createFontImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = fontImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device_.device(), &viewInfo, nullptr, &fontImageView_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create font atlas image view");
    }
}

void TextRenderSystem::createFontSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0F;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = 0.0F;

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &fontSampler_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create font atlas sampler");
    }
}

void TextRenderSystem::createFontDescriptor() {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = fontImageView_;
    imageInfo.sampler = fontSampler_;

    if (!LveDescriptorWriter(*fontSetLayout_, descriptorPool_).writeImage(0, &imageInfo).build(fontDescriptorSet_)) {
        throw std::runtime_error("failed to allocate font atlas descriptor set");
    }
}

void TextRenderSystem::transitionFontImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = device_.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = fontImage_;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage{};
    VkPipelineStageFlags destinationStage{};

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("unsupported font atlas image layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    device_.endSingleTimeCommands(commandBuffer);
}

void TextRenderSystem::render(FrameInfo &frameInfo, const std::vector<GameObjectText> &objects) const {
    render(frameInfo.commandBuffer, frameInfo.camera2d, objects);
}

void TextRenderSystem::render(VkCommandBuffer commandBuffer, const Camera2d &camera,
                              const std::vector<GameObjectText> &objects) const {
    render(commandBuffer, camera.projectionView(), objects);
}

void TextRenderSystem::render(
        VkCommandBuffer commandBuffer,
        const glm::mat4& projectionView,
        const std::vector<GameObjectText> &objects) const {
    pipeline_->bind(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &fontDescriptorSet_,
                            0, nullptr);

    for (const auto &object : objects) {
        TextPushConstantData push{};
        push.projectionView = projectionView;
        push.model = object.transform.mat4();
        vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(TextPushConstantData),
                           &push);

        object.mesh->bind(commandBuffer);
        object.mesh->draw(commandBuffer);
    }
}

} // namespace lve
