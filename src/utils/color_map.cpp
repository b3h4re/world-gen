#include "color_map.hpp"
#include "terrain/generators/generator.hpp"
#include "model/buffer/lve_buffer.hpp"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <stdexcept>
#include <stdexcept>

namespace lve {


/*
So first map [-1; 1] to half circle in complex plain
        -1 |---> -i (blue)
         0 |--->  1 (green)
         1 |--->  i (red)
Which is w(x) = (i - x) / (i + x)
So we get a vector in 2d space with
Re(w) = (1 - x^2) / (1 + x^2)
Im(w) = 2 x / (1 + x^2)

Then convert the resulting vector to HSV and then to RGB
*/
glm::vec3 terrainColor(float height) {
    if (std::abs(height) > 1.0f) {
        throw std::runtime_error("height must be in [-1; 1] to get color mapping.");
    }

    float re = (1 - height * height) / (1 + height * height);
    float im = 2 * height / (1 + height * height);

    float theta = std::atan2(im, re);

    // convert to HSV first, S = 1, V = 1
    // H' = 6H
    float H_ = 6 * (0.333f - 2 * theta * 0.333f / glm::pi<float>());

    // temp value for conversion
    float X = 1 - std::abs(glm::mod<float>(H_, 2) - 1);

    if (H_ < 1) {
        return glm::vec3{1.0f, X, 0.0f};
    }

    if (H_ < 2) {
        return glm::vec3{X, 1.0f, 0.0f};
    }

    if (H_ < 3) {
        return glm::vec3{0.0f, 1.0f, X};
    }

    if (H_ < 4) {
        return glm::vec3{0.0f, X, 1.0f};
    }

    if (H_ < 5) {
        return glm::vec3{X, 0.0f, 1.0f};
    }

    return glm::vec3{1.0f, 0.0f, X};

}


glm::vec3 terrainBlackAndWhite(float height) {
    if (std::abs(height) > 1.0f) {
        throw std::runtime_error("height must be in [-1; 1] to get color mapping.");
    }

    return glm::vec3{(height + 1) / 2};
}

std::vector<std::uint8_t> ColorMapper::generateTerrainColorMapRGBA8(ColorMapParams params) {
    std::vector<std::uint8_t> pixels(params.resolution * 4);
    auto colorFunc = getColorFunction(params.mapping);
    for (std::uint32_t i = 0; i < params.resolution; ++i) {
        float t = static_cast<float>(i) /
                  static_cast<float>(params.resolution - 1);

        float height = wgen::lerp(params.minHeight, params.maxHeight, t);

        glm::vec3 color = colorFunc(height);

        color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));

        pixels[i * 4 + 0] = static_cast<std::uint8_t>(color.r * 255.0f);
        pixels[i * 4 + 1] = static_cast<std::uint8_t>(color.g * 255.0f);
        pixels[i * 4 + 2] = static_cast<std::uint8_t>(color.b * 255.0f);
        pixels[i * 4 + 3] = 255;
    }
    return pixels;
}

ColorMapper::ColorMapper(LveDevice& device) : device_{device} {
    init();
    createColorMap({
        .minHeight = -1.0F,
        .maxHeight = 1.0F,
        .resolution = 512,
        .mapping = ColorFunctions::TerrainColorStandard,
    });
}

ColorMapper::~ColorMapper() {
    clear();
}


void ColorMapper::clear() {
    vkDestroySampler(device_.device(), sampler_, nullptr);
    vkDestroyImageView(device_.device(), imageView_, nullptr);
    vkDestroyImage(device_.device(), image_, nullptr);
    vkFreeMemory(device_.device(), imageMemory_, nullptr);
}


void ColorMapper::init() {
    initSampler(device_);
}


void ColorMapper::recreateColorMap(ColorMapParams params) {
    clear();
    createColorMap(params);
}

VkDescriptorImageInfo ColorMapper::descriptorInfo() const {
    return {
        .sampler = sampler_,
        .imageView = imageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}

void ColorMapper::createColorMap(ColorMapParams params) {
    auto pixels = generateTerrainColorMapRGBA8(params);
    VkDeviceSize imageSize = pixels.size();


    LveBuffer stagingBuffer{
        device_,
        imageSize,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer(pixels.data());

    const uint32_t width = params.resolution;
    const uint32_t height = 1;

    createImage(
        width,
        height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        image_,
        imageMemory_
    );

    transitionImageLayout(
        device_.device(),
        device_.getCommandPool(),
        device_.graphicsQueue(),
        image_,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    copyBufferToImage(
        device_.device(),
        device_.getCommandPool(),
        device_.graphicsQueue(),
        stagingBuffer.getBuffer(),
        image_,
        width,
        height
    );
    transitionImageLayout(
        device_.device(),
        device_.getCommandPool(),
        device_.graphicsQueue(),
        image_,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    imageView_ = createImageView(
        device_.device(),
        image_,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
}

std::function<glm::vec3(float)> ColorMapper::getColorFunction(ColorFunctions f) {
    switch (f) {
        case ColorFunctions::BlackAndWhite:
            return terrainBlackAndWhite;
        case ColorFunctions::TerrainColorStandard:
            return terrainColor;
    }

    throw std::runtime_error("Unsupported color function encountered.");
}

VkSamplerCreateInfo ColorMapper::samplerInfo() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    return samplerInfo;
}

void ColorMapper::initSampler(LveDevice& device) {
    VkSamplerCreateInfo info = samplerInfo();
    VkSampler sampler{};
    if (vkCreateSampler(device.device(), &info, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create color map sampler");
    }

    sampler_ = std::move(sampler);
}



VkImageCreateInfo ColorMapper::imageCreateInfo(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;

    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;

    imageInfo.format = format;
    imageInfo.tiling = tiling;

    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    imageInfo.usage = usage;

    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return imageInfo;
}

uint32_t ColorMapper::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        bool typeMatches = typeFilter & (1 << i);
        bool propertiesMatch =
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;

        if (typeMatches && propertiesMatch) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void ColorMapper::createImage(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageTiling tiling,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags memoryProperties,
            VkImage& image,
            VkDeviceMemory& imageMemory
        ) {

    VkImageCreateInfo imageInfo = imageCreateInfo(width, height, format, tiling, usage);

    if (vkCreateImage(device_.device(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image in color mapper");
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(device_.device(), image, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        device_.physicalDevice(),
        memoryRequirements.memoryTypeBits,
        memoryProperties
    );

    if (vkAllocateMemory(device_.device(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory for color mapper");
    }

    vkBindImageMemory(device_.device(), image, imageMemory, 0);
}

void ColorMapper::transitionImageLayout(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
) {
    VkCommandBuffer commandBuffer = device_.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;

    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage{};
    VkPipelineStageFlags destinationStage{};

    if (
        oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (
        oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported image layout transition");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    device_.endSingleTimeCommands(commandBuffer);
}

void ColorMapper::copyBufferToImage(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height
) {
    VkCommandBuffer commandBuffer = device_.beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    device_.endSingleTimeCommands(commandBuffer);
}

VkImageView ColorMapper::createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags
) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;

    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView{};

    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view");
    }

    return imageView;
}


}
