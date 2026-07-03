#pragma once

#include "device/lve_device.hpp"

#include <cstdint>
#include <vector>
#include <functional>


#include <glm/glm.hpp>
#include <vulkan/vulkan.h>


namespace lve {

glm::vec3 terrainColor(float height);

glm::vec3 terrainBlackAndWhite(float height);

enum class ColorFunctions {
    BlackAndWhite,
    TerrainColorStandard
};

struct ColorMapParams {
    float minHeight;
    float maxHeight;
    std::uint32_t resolution;
    ColorFunctions mapping;
};

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer);

class ColorMapper {
public:
    ColorMapper(LveDevice& device);
    ~ColorMapper();
    static std::vector<std::uint8_t> generateTerrainColorMapRGBA8(ColorMapParams params);

    static std::function<glm::vec3(float)> getColorFunction(ColorFunctions f);

    VkSampler& sampler() { return sampler_; }
    void createColorMap(ColorMapParams params);

    VkDescriptorImageInfo descriptorInfo() const;



private:
    void init();

    VkImage image_;
    VkImageView imageView_;
    VkDeviceMemory imageMemory_;

    VkSampler sampler_;
    LveDevice& device_;
    static VkSamplerCreateInfo samplerInfo();
    static VkImageCreateInfo imageCreateInfo(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage);
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void initSampler(LveDevice& device);

    void createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memoryProperties,
        VkImage& image,
        VkDeviceMemory& imageMemory
    );
    void transitionImageLayout(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue graphicsQueue,
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );
    void copyBufferToImage(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue graphicsQueue,
        VkBuffer buffer,
        VkImage image,
        uint32_t width,
        uint32_t height
    );
    VkImageView createImageView(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags
    );


};

}
